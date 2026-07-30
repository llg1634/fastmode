# Build speedhack DLLs with MinGW (x64). x86 optional if i686 toolchain exists.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Out = Join-Path (Split-Path -Parent (Split-Path -Parent $Root)) "src-tauri\resources"
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$MH = Join-Path $Root "minhook-src"
$Inc = Join-Path $MH "include"
$Src = Join-Path $MH "src"

$mhSources = @(
  "$Src\buffer.c",
  "$Src\hook.c",
  "$Src\trampoline.c",
  "$Src\hde\hde64.c"
)

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
if (-not (Test-Path $gcc)) { $gcc = "gcc" }

$cflags = @("-O2", "-shared", "-static-libgcc", "-I$Inc", "-I$Src", "-DWIN32_LEAN_AND_MEAN")
$ldflags = @("-lwinmm", "-lkernel32", "-luser32")

$out64 = Join-Path $Out "speedhack_x64.dll"
& $gcc $cflags -o $out64 (Join-Path $Root "speedhack.c") $mhSources $ldflags
if ($LASTEXITCODE -ne 0) { throw "x64 build failed" }
Write-Host "Built $out64"

# Also copy as speedhack_x86 name placeholder message
$gcc32 = "C:\msys64\mingw32\bin\gcc.exe"
if (Test-Path $gcc32) {
  $mh32 = @(
    "$Src\buffer.c",
    "$Src\hook.c",
    "$Src\trampoline.c",
    "$Src\hde\hde32.c"
  )
  $out32 = Join-Path $Out "speedhack_x86.dll"
  & $gcc32 $cflags -o $out32 (Join-Path $Root "speedhack.c") $mh32 $ldflags
  Write-Host "Built $out32"
} else {
  Write-Host "No mingw32 gcc; skip speedhack_x86.dll (64-bit targets only for now)"
}

Write-Host "Done."
