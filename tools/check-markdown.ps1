param(
    [string[]]$Paths = @("README.md", "CHANGELOG.md", "ROADMAP.md", "AGENTS.md", "Docs")
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path ".").Path
$failures = New-Object System.Collections.Generic.List[string]

function Resolve-RepoPath {
    param(
        [string]$BaseDir,
        [string]$Target
    )

    $clean = $Target.Trim()
    if ($clean -eq "" -or $clean.StartsWith("#")) {
        return $null
    }
    if ($clean -match "^[a-zA-Z][a-zA-Z0-9+.-]*:") {
        return $null
    }

    $withoutAnchor = ($clean -split "#", 2)[0]
    if ($withoutAnchor -eq "") {
        return $null
    }

    $combined = Join-Path $BaseDir $withoutAnchor
    return [System.IO.Path]::GetFullPath($combined)
}

function Test-WithinRepo {
    param([string]$Path)

    return $Path.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)
}

function Add-Failure {
    param(
        [string]$File,
        [int]$Line,
        [string]$Message
    )

    $script:failures.Add("${File}:${Line}: ${Message}")
}

function Get-RepoRelativePath {
    param([string]$Path)

    if ($Path.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $Path.Substring($root.Length).TrimStart('\', '/')
    }
    return $Path
}

$markdownFiles = New-Object System.Collections.Generic.List[string]
foreach ($path in $Paths) {
    if (Test-Path -LiteralPath $path -PathType Container) {
        Get-ChildItem -LiteralPath $path -Recurse -Filter "*.md" | ForEach-Object {
            $markdownFiles.Add($_.FullName)
        }
    } elseif (Test-Path -LiteralPath $path -PathType Leaf) {
        $markdownFiles.Add((Resolve-Path -LiteralPath $path).Path)
    }
}

$inlinePathPattern = "^(src|include|tests|Docs|examples|tools)/|^(CMakeLists\.txt|Makefile|README\.md|CHANGELOG\.md|ROADMAP\.md|AGENTS\.md|LICENSE|build\.sh)$"

foreach ($file in $markdownFiles) {
    $relativeFile = Get-RepoRelativePath -Path $file
    $baseDir = Split-Path -Parent $file
    $lines = Get-Content -LiteralPath $file -Encoding UTF8

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $lineNo = $i + 1
        $line = $lines[$i]

        foreach ($match in [regex]::Matches($line, "!?\[[^\]]+\]\(([^)]+)\)")) {
            $target = $match.Groups[1].Value.Trim()
            $resolved = Resolve-RepoPath -BaseDir $baseDir -Target $target
            if ($null -eq $resolved) {
                continue
            }
            if (-not (Test-WithinRepo -Path $resolved)) {
                Add-Failure $relativeFile $lineNo "link leaves repository: $target"
            } elseif (-not (Test-Path -LiteralPath $resolved)) {
                Add-Failure $relativeFile $lineNo "broken Markdown link: $target"
            }
        }

        if ($line -match "planned") {
            continue
        }

        foreach ($match in [regex]::Matches($line, '`([^`]+)`')) {
            $candidate = $match.Groups[1].Value.Trim().Replace('\', '/')
            if ($candidate -notmatch $inlinePathPattern) {
                continue
            }
            if ($candidate.Contains("*") -or $candidate.Contains(" ")) {
                continue
            }
            $resolved = Resolve-RepoPath -BaseDir $root -Target $candidate
            if ($null -ne $resolved -and -not (Test-Path -LiteralPath $resolved)) {
                Add-Failure $relativeFile $lineNo "missing inline repo path: $candidate"
            }
        }
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Host $_ }
    exit 1
}

Write-Host "Markdown links and repository paths are valid."
