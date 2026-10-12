$ErrorActionPreference = 'Stop'
$reshadeRoot = $env:RESHADE_ROOT
if (!$reshadeRoot) { throw 'Set RESHADE_ROOT to a ReShade source checkout containing .tools/reshade-api18, .tools/imgui19222 and .tools/llvm-mingw-*.' }
$sdk = Join-Path $reshadeRoot '.tools/reshade-api18/include'
$imgui = Join-Path $reshadeRoot '.tools/imgui19222'
$toolchain = Get-ChildItem (Join-Path $reshadeRoot '.tools/llvm-mingw-*/bin') -Directory | Select-Object -First 1
if (!$toolchain -or !(Test-Path "$sdk/reshade.hpp") -or !(Test-Path "$imgui/imgui.h")) {
    throw 'Requires the official ReShade API 18 headers and matching ImGui 1.92.2b headers in the parent ReShade .tools directory.'
}
$compiler = Join-Path $toolchain.FullName 'x86_64-w64-mingw32-clang++.exe'
$output = Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Force $output | Out-Null
$common = @('-std=c++17','-O2','-Wall','-Wextra','-Wno-unknown-attributes','-Wno-c++20-extensions','-DWIN32_LEAN_AND_MEAN','-DNOMINMAX')
& $compiler @common -shared -static "-I$sdk" "-I$imgui" (Join-Path $PSScriptRoot 'hopout_addon.cpp') -ladvapi32 -lshell32 -o (Join-Path $output 'HopupCreator-Educational.addon64')
if ($LASTEXITCODE -ne 0) { throw 'Hopup add-on compilation failed.' }
& $compiler -std=c++17 -O2 -Wall -Wextra -static (Join-Path $PSScriptRoot 'test_guide.cpp') -o (Join-Path $output 'test_guide.exe')
if ($LASTEXITCODE -ne 0) { throw 'Guide test compilation failed.' }
& (Join-Path $output 'test_guide.exe')
if ($LASTEXITCODE -ne 0) { throw 'Guide tests failed.' }
& $compiler @common -static "-I$sdk" "-I$imgui" (Join-Path $PSScriptRoot 'test_save.cpp') -ladvapi32 -lshell32 -o (Join-Path $output 'test_save.exe')
if ($LASTEXITCODE -ne 0) { throw 'Save workflow test compilation failed.' }
& (Join-Path $output 'test_save.exe')
if ($LASTEXITCODE -ne 0) { throw 'Save workflow tests failed.' }
Copy-Item (Join-Path $PSScriptRoot 'README.md') (Join-Path $output 'README.md') -Force
Write-Output "Built and tested: $output"
