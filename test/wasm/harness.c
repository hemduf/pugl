// Copyright 2026 Pugl contributors
// SPDX-License-Identifier: ISC

#include <emscripten.h>

int
main(void)
{
  emscripten_run_script(
    "const body=document.body;"
    "const params=new URLSearchParams(window.location.search);"
    "const canvas=document.createElement('canvas');"
    "const counts={focus:0,keydown:0,pointerdown:0,pointermove:0,"
    "pointerup:0,resize:0,wheel:0};"
    "canvas.id='pugl-wasm-harness';"
    "canvas.tabIndex=0;"
    "canvas.width=320;"
    "canvas.height=180;"
    "canvas.style.width='320px';"
    "canvas.style.height='180px';"
    "body.appendChild(canvas);"
    "canvas.addEventListener('focus',()=>++counts.focus);"
    "canvas.addEventListener('keydown',()=>++counts.keydown);"
    "canvas.addEventListener('pointerdown',()=>++counts.pointerdown);"
    "canvas.addEventListener('pointermove',()=>++counts.pointermove);"
    "canvas.addEventListener('pointerup',()=>++counts.pointerup);"
    "canvas.addEventListener('wheel',event=>{++counts.wheel;"
    "event.preventDefault();},{passive:false});"
    "window.addEventListener('resize',()=>++counts.resize);"
    "window.puglHarness=counts;"
    "body.dataset.puglReady='true';"
    "if(params.get('fail')==='1'){"
    "body.dataset.puglTest='fail';"
    "body.dataset.puglReason='intentional-harness-self-test';"
    "console.error('Intentional Pugl WASM harness failure');"
    "}else{body.dataset.puglTest='pass';}");

  return 0;
}
