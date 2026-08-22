param(
    [string]$Installer = "",
    [string]$InstallDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Installer)) {
    $Installer = Join-Path $root "dist\installer\nazm-setup-0.4.0-x64.exe"
}
if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $InstallDirectory = Join-Path $env:LOCALAPPDATA "Temp\NazmInstallerContract"
}
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$checksumPath = $Installer + ".sha256"
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    throw "Installer checksum file was not found: $checksumPath"
}
$checksumLine = [IO.File]::ReadAllText(
    $checksumPath, [Text.Encoding]::ASCII).Trim()
if ($checksumLine -notmatch '^([0-9A-Fa-f]{64}) \*(.+)$') {
    throw "Installer checksum file has an invalid format."
}
if ($Matches[2] -cne [IO.Path]::GetFileName($Installer)) {
    throw "Installer checksum names the wrong file."
}
$actualInstallerHash =
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Installer).Hash
if ($Matches[1] -ine $actualInstallerHash) {
    throw "Installer SHA-256 verification failed."
}
$arabicCommand = -join [char[]](0x0646, 0x0638, 0x0645)
$versionArgument = "--" + (-join [char[]](0x0625, 0x0635, 0x062F, 0x0627, 0x0631))

if (Test-Path -LiteralPath $InstallDirectory) {
    throw "Installer test directory already exists: $InstallDirectory"
}

$binDirectory = Join-Path $InstallDirectory "bin"
$arabicExecutable = Join-Path $binDirectory "$arabicCommand.exe"
$portableExecutable = Join-Path $binDirectory "nazm.exe"
$uninstaller = Join-Path $InstallDirectory "unins000.exe"
$markerKey = "HKCU:\Software\BaaEcosystem\Nazm"
$installed = $false

function Wait-InstallerState {
    param(
        [string]$Description,
        [scriptblock]$Condition
    )

    for ($attempt = 0; $attempt -lt 300; $attempt++) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out waiting for $Description."
}

function Invoke-NazmInstaller {
    $setupProcess = Start-Process -FilePath $Installer -ArgumentList @(
        "/VERYSILENT",
        "/SUPPRESSMSGBOXES",
        "/NORESTART",
        "/SP-",
        "/CURRENTUSER",
        "/DIR=$InstallDirectory"
    ) -WindowStyle Hidden -Wait -PassThru
    if ($setupProcess.ExitCode -ne 0) {
        throw "Nazm installer failed with exit code $($setupProcess.ExitCode)."
    }
}

try {
    Invoke-NazmInstaller
    $installed = $true

    Wait-InstallerState "Nazm installation" {
        (Test-Path -LiteralPath $arabicExecutable -PathType Leaf) -and
        (Test-Path -LiteralPath $portableExecutable -PathType Leaf) -and
        (Test-Path -LiteralPath $uninstaller -PathType Leaf) -and
        (Test-Path -LiteralPath $markerKey)
    }
    $staleUpgradeFile = Join-Path $binDirectory "removed-by-upgrade.tmp"
    [IO.File]::WriteAllText($staleUpgradeFile, "stale")
    Invoke-NazmInstaller
    if (Test-Path -LiteralPath $staleUpgradeFile) {
        throw "Nazm repair did not remove an obsolete owned payload file."
    }
    $marker = Get-ItemProperty -LiteralPath $markerKey
    if ($marker.Version -ne '0.4.0' -or
        $marker.InstallLocation -ine $InstallDirectory) {
        throw 'Nazm installer did not record its installed version and location.'
    }

    if (-not (Test-Path -LiteralPath $arabicExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath $portableExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        throw "Nazm installer did not create the required commands or uninstaller."
    }

    $pathValue = (Get-ItemProperty -Path "HKCU:\Environment" -Name Path).Path
    $pathMatches = @($pathValue -split ";" | Where-Object {
        $_.Trim().Trim('"').TrimEnd('\') -ieq $binDirectory.TrimEnd('\')
    })
    if ($pathMatches.Count -ne 1) {
        throw "Nazm installer PATH entry count is $($pathMatches.Count), expected 1."
    }

    $pathOwned = Get-ItemPropertyValue -Path $markerKey -Name PathOwned
    if ($pathOwned -ne 1) { throw "Nazm installer did not record PATH ownership." }

    & $arabicExecutable $versionArgument
    if ($LASTEXITCODE -ne 0) { throw "Installed Nazm version probe failed." }
}
finally {
    if ($installed -and (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        $uninstallProcess = Start-Process -FilePath $uninstaller -ArgumentList @(
            "/VERYSILENT",
            "/SUPPRESSMSGBOXES",
            "/NORESTART"
        ) -WindowStyle Hidden -Wait -PassThru
        if ($uninstallProcess.ExitCode -ne 0) {
            throw "Nazm uninstaller failed with exit code $($uninstallProcess.ExitCode)."
        }
        Wait-InstallerState "Nazm uninstall cleanup" {
            -not (Test-Path -LiteralPath $InstallDirectory) -and
            -not (Test-Path -LiteralPath $markerKey)
        }
    }
}

$remainingPath = (Get-ItemProperty -Path "HKCU:\Environment" -Name Path).Path
$remainingMatches = @($remainingPath -split ";" | Where-Object {
    $_.Trim().Trim('"').TrimEnd('\') -ieq $binDirectory.TrimEnd('\')
})
if ($remainingMatches.Count -ne 0) { throw "Nazm uninstaller left its PATH entry." }
if (Test-Path -LiteralPath $InstallDirectory) { throw "Nazm uninstaller left installation files." }
if (Test-Path -LiteralPath $markerKey) { throw "Nazm uninstaller left its ownership marker." }

Write-Output "Nazm installer contract passed."
