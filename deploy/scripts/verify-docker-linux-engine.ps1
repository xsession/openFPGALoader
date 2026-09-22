[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

try {
    $engine = (& docker info --format '{{.OSType}}/{{.Architecture}}' 2>&1).Trim()
}
catch {
    Write-Error "Docker is unavailable. Start Docker Desktop and retry. $($_.Exception.Message)"
    exit 1
}

if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($engine)) {
    Write-Error "Docker could not report its container engine. Start Docker Desktop and retry."
    exit 1
}

if (-not $engine.StartsWith('linux/', [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error @"
The openFPGALoader Windows cross-build uses an Alpine Linux container, but Docker reports '$engine'.
Switch Docker Desktop to the WSL2/Linux container engine:
  1. Enable 'Use the WSL 2 based engine' in Docker Desktop Settings > General.
  2. Use the Docker Desktop tray menu: 'Switch to Linux containers'.
Then run this check again.
"@
    exit 2
}

Write-Host "Docker engine: $engine"
