# Build the single x64 payload containing process timing and audio speedhack logic.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = Split-Path -Parent (Split-Path -Parent $Root)
$MH = Join-Path $Root "minhook-src"
$ObjectDir = Join-Path $env:TEMP "FastMode-speedhack-x64"
$OutputDll = Join-Path $ObjectDir "speedhack_x64.dll"
$TauriOutput = Join-Path $Repo "src-tauri\resources\speedhack_x64.dll"

New-Item -ItemType Directory -Force -Path $ObjectDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $TauriOutput) | Out-Null

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
if (-not (Test-Path $gcc)) { $gcc = "gcc" }
$gpp = "C:\msys64\mingw64\bin\g++.exe"
if (-not (Test-Path $gpp)) { $gpp = "g++" }

$cflags = @(
  "-O2", "-c",
  "-I$MH\include",
  "-I$Root",
  "-DWIN32_LEAN_AND_MEAN",
  "-D_WIN32_WINNT=0x0601",
  "-DFASTMODE_USE_SOUNDTOUCH"
)
$cSources = @(
  @{ Source = (Join-Path $Root "speedhack.c"); Object = (Join-Path $ObjectDir "speedhack.o") },
  @{ Source = (Join-Path $MH "src\buffer.c"); Object = (Join-Path $ObjectDir "minhook_buffer.o") },
  @{ Source = (Join-Path $MH "src\hook.c"); Object = (Join-Path $ObjectDir "minhook_hook.o") },
  @{ Source = (Join-Path $MH "src\trampoline.c"); Object = (Join-Path $ObjectDir "minhook_trampoline.o") },
  @{ Source = (Join-Path $MH "src\hde\hde64.c"); Object = (Join-Path $ObjectDir "minhook_hde64.o") }
)
$objects = @()
foreach ($item in $cSources) {
  & $gcc @cflags -o $item.Object $item.Source
  if ($LASTEXITCODE -ne 0) { throw "x64 C compile failed: $($item.Source)" }
  $objects += $item.Object
}

$soundTouch = Join-Path $Root "third_party\soundtouch"
$soundTouchSource = Join-Path $soundTouch "source\SoundTouch"
$cppFlags = @(
  "-O3", "-ffast-math", "-std=c++17", "-c",
  "-DSOUNDTOUCH_FLOAT_SAMPLES=1",
  "-I$Root",
  "-I$(Join-Path $soundTouch 'include')"
)
$cppSources = @(
  (Join-Path $Root "soundtouch_bridge.cpp"),
  (Join-Path $soundTouchSource "AAFilter.cpp"),
  (Join-Path $soundTouchSource "cpu_detect_x86.cpp"),
  (Join-Path $soundTouchSource "FIFOSampleBuffer.cpp"),
  (Join-Path $soundTouchSource "FIRFilter.cpp"),
  (Join-Path $soundTouchSource "InterpolateCubic.cpp"),
  (Join-Path $soundTouchSource "InterpolateLinear.cpp"),
  (Join-Path $soundTouchSource "InterpolateShannon.cpp"),
  (Join-Path $soundTouchSource "mmx_optimized.cpp"),
  (Join-Path $soundTouchSource "RateTransposer.cpp"),
  (Join-Path $soundTouchSource "SoundTouch.cpp"),
  (Join-Path $soundTouchSource "sse_optimized.cpp"),
  (Join-Path $soundTouchSource "TDStretch.cpp")
)
for ($i = 0; $i -lt $cppSources.Count; $i++) {
  $object = Join-Path $ObjectDir ("soundtouch_" + $i + ".o")
  & $gpp @cppFlags -o $object $cppSources[$i]
  if ($LASTEXITCODE -ne 0) { throw "x64 C++ compile failed: $($cppSources[$i])" }
  $objects += $object
}

& $gpp -shared -s -static -static-libgcc -static-libstdc++ -o $OutputDll @objects -lwinmm -lpsapi -lole32
if ($LASTEXITCODE -ne 0) { throw "x64 link failed" }

Copy-Item -Force $OutputDll $TauriOutput
Write-Host "Built $TauriOutput"
