[CmdletBinding()]
param(
    [ValidateSet('xbox', 'psn')][string]$Platform,
    [string]$Handle,
    [switch]$Refresh,
    [string]$ImportJson,
    [string]$Out = (Join-Path $PSScriptRoot '..\demos\profiles\profile-snapshot.json')
)

$ErrorActionPreference = 'Stop'
$program = Join-Path $PSScriptRoot '..\tools\prepare_profiles.py'
$programArgs = @($program, '--out', $Out)
if ($Platform) { $programArgs += @('--platform', $Platform) }
if ($Handle) { $programArgs += @('--handle', $Handle) }
if ($ImportJson) { $programArgs += @('--import-json', $ImportJson) }
if ($Refresh) { $programArgs += '--refresh' }

if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 @programArgs
} else {
    & python @programArgs
}
exit $LASTEXITCODE
