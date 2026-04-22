# Enable-VSDevShell.ps1
[CmdletBinding()]
param(
    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'

function Get-VsWherePath {
    $path = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

    if (-not (Test-Path $path)) {
        throw @"
vswhere.exe not found.

Expected location:
  $path

Please make sure Visual Studio Installer / Visual Studio / Build Tools is installed.
"@
    }

    return $path
}

function Get-LaunchVsDevShellPath {
    param(
        [Parameter(Mandatory)]
        [string]$VsWherePath
    )

    $result = & $VsWherePath `
        -latest `
        -products * `
        -find 'Common7\Tools\Launch-VsDevShell.ps1' 2>$null |
        Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($result)) {
        throw "Launch-VsDevShell.ps1 was not found via vswhere."
    }

    if (-not (Test-Path $result)) {
        throw "Launch-VsDevShell.ps1 path returned by vswhere does not exist: $result"
    }

    return $result
}

$vswhere = Get-VsWherePath
$launchScript = Get-LaunchVsDevShellPath -VsWherePath $vswhere

Write-Verbose "vswhere: $vswhere"
Write-Verbose "Launch-VsDevShell.ps1: $launchScript"

# Activate current PowerShell session and keep current directory
. $launchScript -SkipAutomaticLocation

if ($PassThru) {
    [pscustomobject]@{
        VsWherePath          = $vswhere
        LaunchVsDevShellPath = $launchScript
    }
}
