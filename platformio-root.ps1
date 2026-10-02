[CmdletBinding()]
param(
    [ValidateSet('build', 'upload', 'monitor', 'upload-monitor', 'test', 'clean')]
    [string]$Action = 'build',

    [string]$Environment = 'esp32-s3-devkitc-1',

    [string]$Port
)

$ErrorActionPreference = 'Stop'
$rootDirectory = $PSScriptRoot

$platformioExe = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
if (-not (Test-Path -LiteralPath $platformioExe -PathType Leaf)) {
    $platformioCommand = Get-Command platformio -ErrorAction SilentlyContinue
    if ($null -eq $platformioCommand) {
        throw 'PlatformIO was not found. Install PlatformIO Core or the VS Code PlatformIO extension first.'
    }
    $platformioExe = $platformioCommand.Source
}

$environmentHeader = "[env:$Environment]"
$configFiles = @(
    Get-ChildItem -LiteralPath $rootDirectory -Filter 'platformio.ini' -File -Recurse |
        Where-Object {
            $_.FullName -notmatch '[\\/]\.pio[\\/]' -and
            $_.FullName -notmatch '[\\/]node_modules[\\/]'
        }
)

if ($configFiles.Count -eq 0) {
    throw "No platformio.ini was found below '$rootDirectory'."
}

$matchingConfigs = @(
    $configFiles | Where-Object {
        Select-String -LiteralPath $_.FullName -SimpleMatch $environmentHeader -Quiet
    }
)

if ($matchingConfigs.Count -eq 1) {
    $configFile = $matchingConfigs[0]
}
elseif ($matchingConfigs.Count -gt 1) {
    $paths = $matchingConfigs.FullName -join [Environment]::NewLine
    throw "More than one PlatformIO project defines $environmentHeader. Matching files:$([Environment]::NewLine)$paths"
}
elseif ($configFiles.Count -eq 1) {
    $configFile = $configFiles[0]
}
else {
    $paths = $configFiles.FullName -join [Environment]::NewLine
    throw "Could not determine which PlatformIO project to use. Files found:$([Environment]::NewLine)$paths"
}

$projectDirectory = $configFile.DirectoryName
Write-Host "PlatformIO project: $projectDirectory" -ForegroundColor Cyan
Write-Host "Environment:        $Environment" -ForegroundColor Cyan

function Invoke-PlatformIO {
    param([string[]]$Arguments)

    & $platformioExe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO exited with code $LASTEXITCODE."
    }
}

switch ($Action) {
    'build' {
        Invoke-PlatformIO @('run', '--project-dir', $projectDirectory, '--environment', $Environment)
    }
    'upload' {
        Invoke-PlatformIO @('run', '--project-dir', $projectDirectory, '--environment', $Environment, '--target', 'upload')
    }
    'clean' {
        Invoke-PlatformIO @('run', '--project-dir', $projectDirectory, '--environment', $Environment, '--target', 'clean')
    }
    'test' {
        Invoke-PlatformIO @('test', '--project-dir', $projectDirectory, '--environment', $Environment)
    }
    'monitor' {
        Push-Location -LiteralPath $projectDirectory
        try {
            $arguments = @('device', 'monitor', '--environment', $Environment)
            if ($Port) { $arguments += @('--port', $Port) }
            Invoke-PlatformIO $arguments
        }
        finally {
            Pop-Location
        }
    }
    'upload-monitor' {
        Invoke-PlatformIO @('run', '--project-dir', $projectDirectory, '--environment', $Environment, '--target', 'upload')
        Push-Location -LiteralPath $projectDirectory
        try {
            $arguments = @('device', 'monitor', '--environment', $Environment)
            if ($Port) { $arguments += @('--port', $Port) }
            Invoke-PlatformIO $arguments
        }
        finally {
            Pop-Location
        }
    }
}
