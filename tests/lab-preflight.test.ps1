$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../internal/lab-preflight.ps1')

$fixture = [IO.Directory]::CreateDirectory((Join-Path ([IO.Path]::GetTempPath()) ('hy3x-preflight-' + [guid]::NewGuid().ToString('N')))).FullName
$steamManifest = Join-Path $fixture 'appmanifest_730.acf'
$game = Join-Path $fixture 'common/CS2/game/bin/win64/cs2.exe'
$internal = Join-Path $fixture 'internal'
$packageManifest = Join-Path $fixture 'manifest.json'

function Confirm-Rejected([scriptblock]$Action, [string]$Message) {
    try { $null = & $Action }
    catch {
        if ($_.Exception.Message -like $Message) { return }
        throw
    }
    throw "Expected rejection: $Message"
}

try {
    Set-Content -LiteralPath $steamManifest -Value '"installdir" "CS2"', '"buildid" "26598758"'
    if ((Confirm-LabGameBuild $game) -ne '26598758') { throw 'Known build was not accepted.' }
    Set-Content -LiteralPath $steamManifest -Value '"installdir" "CS2"', '"buildid" "26598759"'
    Confirm-Rejected { Confirm-LabGameBuild $game } '*26598759*26598758*'
    Set-Content -LiteralPath $steamManifest -Value '"installdir" "Other"', '"buildid" "26598758"'
    Confirm-Rejected { Confirm-LabGameBuild $game } '*outra instala*'
    Remove-Item -LiteralPath $steamManifest
    Confirm-Rejected { Confirm-LabGameBuild $game } '*Manifesto Steam*ausente*'

    $null = New-Item -ItemType Directory -Path $internal
    $module = Join-Path $internal 'CS.dll'
    [IO.File]::WriteAllBytes($module, [byte[]]@(1, 2, 3))
    $hash = (Get-FileHash -LiteralPath $module).Hash
    Confirm-Rejected { Confirm-LabModule $internal (Get-Item -LiteralPath $module) } '*manifest.json ausente*'
    $null = New-Item -ItemType File -Path (Join-Path $internal 'vc.sln')
    if ((Confirm-LabModule $internal (Get-Item -LiteralPath $module)) -notlike '*desenvolvimento*') {
        throw 'Development checkout was not recognized.'
    }
    @{ files = @(@{ path = 'internal/x64/Release/CS.dll'; sha256 = $hash }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $packageManifest
    if ((Confirm-LabModule $internal (Get-Item -LiteralPath $module)) -notlike '*conferido*') {
        throw 'Matching DLL hash was not accepted.'
    }
    [IO.File]::WriteAllBytes($module, [byte[]]@(1, 2, 4))
    Confirm-Rejected { Confirm-LabModule $internal (Get-Item -LiteralPath $module) } '*CS.dll diverge*'
    'PASS: Steam build and packaged DLL checks.'
} finally {
    Get-ChildItem -LiteralPath $fixture -File -Recurse | Remove-Item -Force
    if (Test-Path -LiteralPath $internal) { Remove-Item -LiteralPath $internal }
    Remove-Item -LiteralPath $fixture
}
