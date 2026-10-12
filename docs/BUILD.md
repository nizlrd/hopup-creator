# Build notes

The add-on is a native ReShade API 18 module. Use the official ReShade Add-on build and matching ImGui docking headers. The current source adapter validates the FiveM executable name and instruction bytes for GTA build 3258; unsupported builds intentionally remain inactive.

The build script is designed to run from a checkout alongside the ReShade source tree. It does not download dependencies, access FiveConfig, read environment secrets or publish binaries.
