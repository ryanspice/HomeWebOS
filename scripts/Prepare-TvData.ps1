param(
    [ValidateSet('Collect','Verify','Stage','Inspect')][string]$Mode='Collect',
    [string]$BundleRoot,
    [string]$Device,
    [string]$AppId,
    [switch]$PrivateReads,
    [string]$MarketingJson,
    [string]$StageDirectory,
    [string]$CliRoot,
    [ValidateSet('public','private')][string]$LunaClient='public'
)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$preparer=Join-Path $project 'tools\prepare_tv_data.py'
if(-not(Test-Path -LiteralPath $preparer -PathType Leaf)){throw 'Preparation tool missing'}
$taskPython=Get-Command py -ErrorAction SilentlyContinue
$arguments=@()
if($taskPython){$arguments+=@('-3')}else{$taskPython=Get-Command python3 -ErrorAction Stop}
$arguments+=@($preparer,$Mode.ToLowerInvariant())
if($Mode -ne 'Inspect'){
    if(-not $BundleRoot){throw 'Collect, Verify and Stage require -BundleRoot'}
    $arguments+=@('--bundle',[IO.Path]::GetFullPath($BundleRoot))
}
if($Mode -in @('Collect','Inspect')){
    if(-not $Device){throw 'Collect requires an explicit configured -Device name'}
    $arguments+=@('--device',$Device)
    $arguments+=@('--luna-client',$LunaClient)
    if($AppId){$arguments+=@('--app',$AppId)}
    if($PrivateReads){$arguments+='--private-reads'}
    if($MarketingJson){$arguments+=@('--marketing',[IO.Path]::GetFullPath($MarketingJson))}
    if($CliRoot){$arguments+=@('--cli-root',[IO.Path]::GetFullPath($CliRoot))}
}elseif($Mode -eq 'Stage'){
    if(-not $StageDirectory){throw 'Stage requires a new -StageDirectory'}
    $arguments+=@('--out',[IO.Path]::GetFullPath($StageDirectory))
}
# Uses existing Python/Pillow/Node/LG CLI. Never installs dependencies, pairs a
# device, changes profiles/keys, writes the TV or invokes an app installation.
& $taskPython.Source @arguments
if($LASTEXITCODE -ne 0){throw 'Preparation failed; previous committed snapshot retained'}
