param(
    [ValidateSet('baseline','expanded')][string]$Profile = 'expanded',
    [ValidateSet('oci','process')][string]$Mode = 'oci',
    [ValidateSet('dds','udp')][string]$Transport = 'dds',
    [switch]$Observe,
    [switch]$BuildOnly,
    [string]$Distro = 'Ubuntu-22.04'
)
$ErrorActionPreference = 'Stop'
if (-not $BuildOnly) {
    $planArgs = @('--profile',$Profile,'--mode',$Mode,'--transport',$Transport)
    if ($Observe) { $planArgs += '--observe' }
    & python -X utf8 (Join-Path $PSScriptRoot 'tools/windows/run.py') @planArgs
    if ($LASTEXITCODE -ne 0) { throw 'Static plan validation failed.' }
    return
}
& python -X utf8 (Join-Path $PSScriptRoot 'tools/windows/layout.py') (Join-Path $PSScriptRoot 'config')
if ($LASTEXITCODE -ne 0) { throw 'Static layout generation failed.' }
$taskRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$taskLinux = '/mnt/' + $taskRoot.Substring(0,1).ToLowerInvariant() + $taskRoot.Substring(2).Replace('\','/')
& wsl.exe -d $Distro --exec /bin/bash "$taskLinux/build.sh"
if ($LASTEXITCODE -ne 0) { throw 'Linux build failed.' }
& wsl.exe -d $Distro --exec /bin/bash "$taskLinux/check_offline.sh"
if ($LASTEXITCODE -ne 0) { throw 'Offline C tests failed.' }
& python -X utf8 (Join-Path $PSScriptRoot 'tools/windows/validate.py')
if ($LASTEXITCODE -ne 0) { throw 'Offline plan checks failed.' }
Write-Host 'Build and offline checks complete.'
