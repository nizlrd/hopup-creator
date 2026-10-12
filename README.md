# Hopup Creator — Educational Edition

Hopup Creator is a free, open-source learning tool for placing your own visual stand and look markers in FiveM. It helps you study and document movement routes such as jumps, mantles and vault approaches. You perform every movement yourself.

This repository is separate from the paid FiveConfig build. It has no account, subscription, lease, device binding, backend connection or production keys.

## Server rules come first

Some servers prohibit third-party assets, ReShade add-ons or route guides. Always read the current rules for the server you are joining. Arena and EchoRP are examples the author has been told may restrict this type of tool; policies change and this project does not make a current approval claim.

Do not install, enable or use Hopup Creator where the server rules prohibit it. Disable it and remove the add-on before joining those servers. You are responsible for following server and platform rules.

See [SERVER-RULES.md](SERVER-RULES.md) for the checklist.

## What it does

- Captures a standing position and look target with F6.
- Saves named `.hopout` route files beside the add-on.
- Displays checked routes nearby with editable labels, instructions, colours, shapes and distance fades.
- Lets you toggle, edit and share each hopup route from the simple list. Share opens the saved `.hopout` file so it can be copied to another learner.
- Shows the closest active hopup's instructions in a top-left guide box, tied to that hopup's visibility distance and toggle state.
- Offers an optional dot or simple wireframe vehicle placement guide with a custom label such as `PUT REVOLTER HERE`.
- Supports optional multi-step routes, aimed placement and wall anchored notes under Advanced options.
- F11 hides all overlays and pauses reads; F12 saves the current route.
- Reads only the local process position and camera. It does not write game memory, automate movement, simulate input, call game natives or make network requests.

It currently targets FiveM GTA build 3258 and ReShade API 18. It does not detect mantle animations, perform physics raycasts, place vehicles or promise that a marker is reachable. The vehicle outline is an approximate visual reference that you position manually.

## Install and build

### Download and drag into plugins

For a ready-to-use build, download the latest [HopupCreator-Educational.zip](https://github.com/nizlrd/hopup-creator/releases/latest/download/HopupCreator-Educational.zip). Close FiveM, extract `HopupCreator-Educational.addon64`, and drag that file into the ReShade add-on/plugins folder you already use. Start FiveM and enable it under ReShade > Add-ons. The package includes this guide and the server-rules checklist.

When the repository is updated, a new release package is published at the same **latest** link. Download the latest package and replace the old `.addon64` with FiveM closed; installed files do not silently self-update.

This source expects the official ReShade **Add-on** build and matching API 18/ImGui 1.92.2b headers. Obtain those from the official ReShade project; they are not redistributed here. The included build script uses the parent ReShade checkout's `.tools` toolchain.

From this repository, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The script builds `bin/HopupCreator-Educational.addon64` and runs the parser and save workflow tests. With FiveM closed, copy the add-on into your existing ReShade add-on folder and open ReShade > Add-ons. Remove it with the game closed. Do not redistribute ReShade binaries or headers from this repository.

## Basic guide

1. Install the official ReShade Add-on build for your permitted test environment.
2. Load the add-on and enable it for the current session.
3. Choose **Create a hopup**, name it, stand at the starting point and press F6.
4. Aim at the required look point and press F6 again.
5. Add optional instructions, then press F12 to save. Tick **Show** to display it nearby.
6. Close the ReShade menu while moving or aiming. Use F11 to hide the guide before streaming.

The route files are plain text and local. Back them up before editing or sharing.

## Development and scope

Contributions should preserve the read-only boundary, fail closed on unsupported game builds, and keep server-rule warnings visible. Please do not add automation, memory writes, input simulation, bypasses or online telemetry.

## License

Original project code is released under the MIT License. The `vendor/` notices document upstream provenance retained from the source project. ReShade and its headers remain under their own licenses; this repository does not grant rights to redistribute them.
