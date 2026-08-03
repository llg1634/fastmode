param(
    [string]$Version = "0.3.0-preview",
    [string]$ReleaseRoot = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ReleaseRoot)) {
    $ReleaseRoot = Join-Path (Split-Path -Parent $repoRoot) "FastMode-GitHub-Release"
}
$project = Join-Path $repoRoot "wpf\FastMode.Desktop\FastMode.Desktop.csproj"
$packageName = "FastMode-v$Version-win-x64"
$releaseRootFull = [System.IO.Path]::GetFullPath($ReleaseRoot)
$packageDir = [System.IO.Path]::GetFullPath((Join-Path $releaseRootFull $packageName))
$zipPath = [System.IO.Path]::GetFullPath((Join-Path $releaseRootFull "$packageName.zip"))

if (-not $packageDir.StartsWith($releaseRootFull + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Package directory must remain inside the release root."
}

New-Item -ItemType Directory -Force -Path $releaseRootFull | Out-Null
if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

$publishArgs = @(
    "publish",
    $project,
    "-c", "Release",
    "-r", "win-x64",
    "--self-contained", "true",
    "-p:PublishSingleFile=false",
    "-p:DebugType=None",
    "-p:DebugSymbols=false",
    "-o", $packageDir
)
& dotnet @publishArgs

Get-ChildItem -LiteralPath $packageDir -Filter *.pdb -File | Remove-Item -Force

Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination (Join-Path $packageDir "README.md")
Copy-Item -LiteralPath (Join-Path $repoRoot "README_EN.md") -Destination (Join-Path $packageDir "README_EN.md")

$releaseNotesSource = Join-Path $repoRoot "release\RELEASE_NOTES_v0.3.0-preview.md"
$releaseNotesRoot = Join-Path $releaseRootFull "RELEASE_NOTES_v0.3.0-preview.md"
Copy-Item -LiteralPath $releaseNotesSource -Destination $releaseNotesRoot
Copy-Item -LiteralPath $releaseNotesSource -Destination (Join-Path $packageDir "RELEASE_NOTES.md")

$assetsDir = Join-Path $packageDir "assets"
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "assets\FastMode.png") -Destination $assetsDir
Copy-Item -LiteralPath (Join-Path $repoRoot "assets\screenshot-main.png") -Destination $assetsDir

$licensesDir = Join-Path $packageDir "licenses"
New-Item -ItemType Directory -Force -Path $licensesDir | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "native\speedhack\minhook-src\LICENSE.txt") -Destination (Join-Path $licensesDir "MinHook-LICENSE.txt")
Copy-Item -LiteralPath (Join-Path $repoRoot "native\speedhack\third_party\soundtouch\COPYING.TXT") -Destination (Join-Path $licensesDir "SoundTouch-COPYING.txt")

Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination (Join-Path $releaseRootFull "README.md")
Copy-Item -LiteralPath (Join-Path $repoRoot "README_EN.md") -Destination (Join-Path $releaseRootFull "README_EN.md")

Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -CompressionLevel Optimal

$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
$exeHash = (Get-FileHash -LiteralPath (Join-Path $packageDir "FastMode.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
$dllHash = (Get-FileHash -LiteralPath (Join-Path $packageDir "speedhack_x64.dll") -Algorithm SHA256).Hash.ToLowerInvariant()
$newLines = @(
    "$zipHash  $packageName.zip",
    "$exeHash  $packageName/FastMode.exe",
    "$dllHash  $packageName/speedhack_x64.dll"
)

$versionChecksums = Join-Path $releaseRootFull "SHA256SUMS_v0.3.0-preview.txt"
$newLines | Set-Content -LiteralPath $versionChecksums -Encoding utf8

$aggregateChecksums = Join-Path $releaseRootFull "SHA256SUMS.txt"
$existingLines = if (Test-Path -LiteralPath $aggregateChecksums) {
    Get-Content -LiteralPath $aggregateChecksums | Where-Object { $_ -notmatch [regex]::Escape($packageName) }
} else {
    @()
}
@($existingLines) + $newLines | Set-Content -LiteralPath $aggregateChecksums -Encoding utf8

[pscustomobject]@{
    PackageDirectory = $packageDir
    Zip = $zipPath
    ZipSize = (Get-Item -LiteralPath $zipPath).Length
    ZipSha256 = $zipHash
    ExeSha256 = $exeHash
    SpeedhackSha256 = $dllHash
}
