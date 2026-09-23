# iOS examples

These examples exercise the iOS backend in the two integration modes that matter
most for Pugl:

- **PuglStandalone** uses a `PUGL_PROGRAM` world and lets Pugl own its top-level
  `UIWindow`.
- **PuglAUv3Extension** uses a `PUGL_MODULE` world embedded in the
  `AUViewController` supplied by an Audio Unit v3 host.  The companion
  **PuglAUv3Container** application exists so the extension can be installed.

The examples intentionally use `puglStubBackend()`.  This keeps the sample
focused on UIKit lifecycle, embedding, touch/pointer events, focus, hardware
keyboard input, multiple instances, and AUv3 host ownership.  A renderer can be
added independently.

## Generate the Xcode project

The project is generated with [XcodeGen](https://github.com/yonaskolb/XcodeGen):

```sh
brew install xcodegen
cd examples/ios
xcodegen generate
open PuglIOSExamples.xcodeproj
```

No generated Xcode project is committed.  `project.yml` is the source of
truth.

## Standalone app

Select the **PuglStandalone** scheme and run it on an iPhone/iPad simulator or
device.

The sample creates a `PUGL_PROGRAM` world, shows a top-level Pugl view, and
prints lifecycle/input state in the view.  Touching the view toggles Pugl's explicit text-input session.  The first tap
requests logical focus and starts text input; a later tap stops it.  This can
be used with the software keyboard or a connected hardware keyboard.

## AUv3

Build/run **PuglAUv3Container** once to install the extension, then instantiate
**Pugl iOS Example** from an AUv3 host.

Each `PuglExampleAudioUnitViewController` owns an independent
`PUGL_MODULE` world and embeds its Pugl view into the host-provided
`UIView`.  Touching that embedded view toggles its per-instance text-input
session.  No `UIApplication` ownership or process-global editor/view state
is used.

The audio unit is a minimal stereo pass-through effect.  Its render block only
pulls input into the host-provided output buffers; the Pugl/UI code never runs
on the real-time render thread.

## Runtime class isolation

Pugl is compiled directly into each final consumer target with a different
consumer-specific `PUGL_OBJC_CLASS_PREFIX`:

- `PuglStandaloneExample` for the standalone app;
- `PuglAUv3Example` for the AUv3 extension.

This mirrors the static-link/plugin coexistence requirements of the iOS
backend.

## CI

The repository iOS workflow generates this project and builds both the
standalone app and the AUv3 container/extension for arm64 simulator and device
SDKs with signing disabled.
