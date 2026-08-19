param(
    [string]$Version = "0.4.0",
    [string]$BuildDirectory = "",
    [string]$IsccPath = "",
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$setupScript = Join-Path $root "setup.iss"
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $root "build\windows-release"
}

$cmakeText = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root "CMakeLists.txt")
$headerText = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root "include\nazm.h")
if ($cmakeText -notmatch "VERSION\s+$([regex]::Escape($Version))" -or
    $headerText -notmatch "NAZM_VERSION_STRING\s+`"$([regex]::Escape($Version))`"") {
    throw "Nazm version $Version is not synchronized across CMakeLists.txt and include/nazm.h."
}

if (-not $SkipBuild) {
    & cmake -S $root -B $BuildDirectory -G "MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release -DNAZM_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw "Nazm CMake configure failed." }

    & cmake --build $BuildDirectory --clean-first
    if ($LASTEXITCODE -ne 0) { throw "Nazm build failed." }
}

if (-not $SkipTests) {
    & ctest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Nazm tests failed." }
}

$arabicCommand = -join [char[]](0x0646, 0x0638, 0x0645)
$requiredFiles = @("$arabicCommand.exe", "nazm.exe", "libnazm.a")
foreach ($file in $requiredFiles) {
    $path = Join-Path $BuildDirectory $file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required Nazm installer input is missing: $path"
    }
}

if ([string]::IsNullOrWhiteSpace($IsccPath)) {
    $isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($isccCommand) { $IsccPath = $isccCommand.Source }
}
if ([string]::IsNullOrWhiteSpace($IsccPath)) {
    $IsccPath = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($IsccPath) -or
    -not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) {
    throw "Inno Setup 6 compiler was not found. Pass -IsccPath explicitly."
}

$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
Push-Location $root
try {
    & $IsccPath "/DMyAppVersion=$Version" `
        "/DNazmBinaryDir=$resolvedBuildDirectory" $setupScript
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE." }
}
finally {
    Pop-Location
}

$installer = Join-Path $root "dist\installer\nazm-setup-$Version-x64.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Nazm installer was not produced at $installer"
}
$installerHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash
$checksum = $installer + ".sha256"
[IO.File]::WriteAllText(
    $checksum,
    "$installerHash *$([IO.Path]::GetFileName($installer))`n",
    [Text.Encoding]::ASCII)
Write-Output $installer
Write-Output $checksum
