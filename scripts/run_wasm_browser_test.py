#!/usr/bin/env python3
# Copyright 2026 Pugl contributors
# SPDX-License-Identifier: ISC

"""Run one generated Emscripten test page in headless Chromium."""

from __future__ import annotations

import argparse
import functools
import http.server
import pathlib
import threading
import urllib.parse

from playwright.sync_api import Error as PlaywrightError
from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("page", type=pathlib.Path)
    parser.add_argument("--query", default="")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--passive", action="store_true")
    mode.add_argument("--pugl-input", action="store_true")
    parser.add_argument(
        "--diagnostics-dir",
        type=pathlib.Path,
        default=pathlib.Path("browser-diagnostics"),
    )
    return parser.parse_args()


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *args: object) -> None:
        del args


def write_diagnostics(page: object, directory: pathlib.Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    try:
        directory.joinpath("page.html").write_text(page.content(), encoding="utf-8")
        page.screenshot(path=str(directory / "page.png"), full_page=True)
    except PlaywrightError as error:
        directory.joinpath("diagnostics-error.txt").write_text(
            f"{error}\n", encoding="utf-8"
        )


def exercise_generic_harness(page: object) -> None:
    canvas = page.locator("#pugl-wasm-harness")
    canvas.focus()
    page.keyboard.press("a")

    box = canvas.bounding_box()
    if not box:
        raise RuntimeError("Harness canvas has no bounding box")

    x = box["x"] + (box["width"] / 2.0)
    y = box["y"] + (box["height"] / 2.0)
    page.mouse.move(x, y)
    page.mouse.down()
    page.mouse.move(x + 8.0, y + 6.0)
    page.mouse.up()
    page.mouse.wheel(0.0, 120.0)

    page.set_viewport_size({"width": 640, "height": 480})
    page.evaluate("window.dispatchEvent(new Event('resize'))")
    page.wait_for_function(
        """
        window.puglHarness &&
        window.puglHarness.focus >= 1 &&
        window.puglHarness.keydown >= 1 &&
        window.puglHarness.pointerdown >= 1 &&
        window.puglHarness.pointermove >= 1 &&
        window.puglHarness.pointerup >= 1 &&
        window.puglHarness.wheel >= 1 &&
        window.puglHarness.resize >= 1
        """,
        timeout=5000,
    )


def exercise_pugl_input(page: object) -> None:
    text_input = page.locator("textarea[data-pugl-text-input]")
    if text_input.count() != 1:
        raise RuntimeError("Expected exactly one Pugl browser text input")

    text_input.focus()
    page.keyboard.press("a")
    page.keyboard.down("Shift")
    try:
        page.keyboard.insert_text("É")
    finally:
        page.keyboard.up("Shift")

    page.keyboard.press("Tab")
    if not text_input.evaluate("element => document.activeElement === element"):
        raise RuntimeError("Tab moved browser focus away from the Pugl view")

    finish_result = page.evaluate(
        """
        () => {
          if (typeof Module !== 'object' ||
              typeof Module['_puglWasmInputFinish'] !== 'function') {
            throw new Error('Missing _puglWasmInputFinish export');
          }
          return Module['_puglWasmInputFinish']();
        }
        """
    )
    if finish_result != 0:
        raise RuntimeError(f"Pugl input finalizer returned {finish_result}")

    page.wait_for_function(
        "document.body && document.body.dataset.puglTest !== 'pending'",
        timeout=5000,
    )


def main() -> int:
    args = parse_args()
    test_page = args.page.resolve()
    if not test_page.is_file():
        print(f"Missing browser test page: {test_page}")
        return 2

    root = test_page.parent
    handler = functools.partial(QuietHandler, directory=str(root))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    console_errors: list[str] = []
    page_errors: list[str] = []
    query = f"?{args.query}" if args.query else ""
    url = (
        f"http://127.0.0.1:{server.server_address[1]}/"
        f"{urllib.parse.quote(test_page.name)}{query}"
    )

    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True)
            page = browser.new_page(viewport={"width": 800, "height": 600})
            page.on(
                "console",
                lambda message: console_errors.append(message.text)
                if message.type == "error"
                else None,
            )
            page.on("pageerror", lambda error: page_errors.append(str(error)))

            try:
                page.goto(url, wait_until="load", timeout=15000)
                page.wait_for_function(
                    "document.body && document.body.dataset.puglReady === 'true'",
                    timeout=10000,
                )

                if args.pugl_input:
                    exercise_pugl_input(page)
                elif not args.passive:
                    exercise_generic_harness(page)

                result = page.locator("body").get_attribute("data-pugl-test")
                reason = page.locator("body").get_attribute("data-pugl-reason")

                if result != "pass" or console_errors or page_errors:
                    print(f"Browser test result: {result!r}")
                    if reason:
                        print(f"Reason: {reason}")
                    for message in console_errors:
                        print(f"Console error: {message}")
                    for message in page_errors:
                        print(f"Page error: {message}")
                    write_diagnostics(page, args.diagnostics_dir)
                    return 1

                if args.pugl_input:
                    print("Pugl input browser test passed")
                elif args.passive:
                    print("Browser test passed")
                else:
                    counts = page.evaluate("window.puglHarness")
                    print(f"Browser harness passed: {counts}")
                return 0
            except (PlaywrightError, PlaywrightTimeoutError, RuntimeError) as error:
                print(f"Browser harness error: {error}")
                for message in console_errors:
                    print(f"Console error: {message}")
                for message in page_errors:
                    print(f"Page error: {message}")
                write_diagnostics(page, args.diagnostics_dir)
                return 1
            finally:
                browser.close()
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5.0)


if __name__ == "__main__":
    raise SystemExit(main())
