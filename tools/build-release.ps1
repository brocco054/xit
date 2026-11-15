[CmdletBinding(DefaultParameterSetName = 'Build')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Build')]
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?$')]
    [string]$Version,

    [Parameter(ParameterSetName = 'Build')]
    [switch]$ObserveItems,

    [Parameter(Mandatory, ParameterSetName = 'Verify')]
    [string]$VerifyDirectory
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program falhou (código $LASTEXITCODE)." }
}

function Get-Inventory([string]$Directory) {
    @(Get-ChildItem -LiteralPath $Directory -File -Recurse | ForEach-Object {
        [pscustomobject][ordered]@{
            path = $_.FullName.Substring($Directory.Length + 1).Replace('\', '/')
            bytes = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        }
    } | Sort-Object { $_.path })
}

function Confirm-Inventory([string]$Directory, [object[]]$Inventory) {
    $actual = @(Get-Inventory $Directory | Where-Object path -ne 'manifest.json')
    $differences = Compare-Object -ReferenceObject $Inventory -DifferenceObject $actual -Property path, bytes, sha256
    if ($differences) {
        $paths = ($differences.path | Sort-Object -Unique) -join ', '
        throw "Conteúdo extraído diverge do manifesto. Arquivos: $paths"
    }
}

if ($PSCmdlet.ParameterSetName -eq 'Verify') {
    $directory = Get-Item -LiteralPath $VerifyDirectory
    if (!$directory.PSIsContainer -or $directory.PSProvider.Name -ne 'FileSystem') {
        throw 'Informe a pasta do pacote Windows x64 extraído.'
    }
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $directory.FullName 'manifest.json') | ConvertFrom-Json
    $inventory = @($manifest.files)
    $requiredPaths = @('Acervo-Laboratorio.exe', 'internal/lab-load.ps1',
        'internal/x64/Release/lab-loader.exe', 'internal/x64/Release/CS.dll')
    if (!$manifest.version -or ($requiredPaths | Where-Object { $_ -notin $inventory.path })) {
        throw 'O manifesto não descreve um pacote Windows x64 completo.'
    }
    Confirm-Inventory $directory.FullName $inventory
    Write-Output "Pacote Hy3x $($manifest.version) verificado ($($inventory.Count) arquivos)."
    return
}

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
$release = Join-Path $dist "Hy3x-$Version"
$vcVersion = '14.44.35207'
$sdkVersion = '10.0.26100.0'
if (Test-Path -LiteralPath $release) { throw "Versão já publicada localmente: $release" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2022 com ferramentas C++ x64 não encontrado.' }
$devCmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$msbuild = Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$compilerEnvironment = & $env:ComSpec /d /c "call `"$devCmd`" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=14.44 -winsdk=$sdkVersion && set"
if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar as ferramentas C++ x64.' }
foreach ($line in $compilerEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
if ($env:VCToolsVersion.TrimEnd('\') -ne $vcVersion -or $env:WindowsSDKVersion.TrimEnd('\') -ne $sdkVersion) {
    throw "Instale MSVC $vcVersion e Windows SDK $sdkVersion."
}

$staging = Join-Path $dist ('.release-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $staging 'source'
$work = Join-Path $staging 'work'
$payload = Join-Path $staging 'package'
$publication = Join-Path $staging 'published'
$native = Join-Path $source 'internal'
$binaries = Join-Path $payload 'internal/x64/Release'
foreach ($directory in @($source, $work, $native, $binaries, $publication)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
foreach ($path in @('.gitignore', 'SKILL.md', 'docs', 'src', 'tests', 'tools', 'ui', 'acervo-case-hardened.cfg')) {
    Copy-Item -LiteralPath (Join-Path $root $path) -Destination $source -Recurse
}
$nativeSource = Join-Path $root 'internal'
foreach ($path in @('.gitignore', 'assets', 'directx', 'docs', 'features', 'hooks', 'menu', 'sdk', 'tools', 'utils',
    'valve', 'acervo-launcher.cpp', 'config.hpp', 'entry.cpp', 'lab-attempt.ps1', 'lab-load.ps1', 'lab-loader.cpp',
    'lab-preflight.ps1', 'lab-window.ps1', 'LICENSE',
    'main.hpp', 'README.md', 'vc.sln', 'vc.vcxproj')) {
    Copy-Item -LiteralPath (Join-Path $nativeSource $path) -Destination $native -Recurse
}

$repository = Join-Path $work 'source.git'
Invoke-Checked 'git' @('init', '--bare', '--initial-branch=main', $repository)
$sourceGit = @('-C', $source, "--git-dir=$repository", "--work-tree=$source", '-c', 'core.autocrlf=false')
Invoke-Checked 'git' ($sourceGit + @('add', '-f', '--all', '--', '.'))
$sourceTree = & git @sourceGit write-tree
if ($LASTEXITCODE -ne 0) { throw 'Falha ao registrar o conteúdo da fonte.' }
Invoke-Checked 'git' ($sourceGit + @('-c', 'user.name=Acervo Build', '-c', 'user.email=acervo-build@localhost',
    'commit', '--quiet', '-m', "Hy3x $Version"))
$sourceCommit = & git @sourceGit rev-parse HEAD
Invoke-Checked 'git' ($sourceGit + @('tag', "v$Version"))
Invoke-Checked 'git' ($sourceGit + @('bundle', 'create', "$publication/Hy3x-$Version-source.bundle", '--all'))
Invoke-Checked 'git' ($sourceGit + @('bundle', 'verify', "$publication/Hy3x-$Version-source.bundle"))

$env:PYTHONDONTWRITEBYTECODE = '1'
Push-Location $source
try {
    & ./tests/lab-attempt.test.ps1
    & ./tests/lab-preflight.test.ps1
    & ./tests/lab-status.test.ps1
    Invoke-Checked 'python' @('internal/tools/catalog_outputs.py', '--check')
    Invoke-Checked 'node' @('--test', '--test-isolation=none', 'tests/profile-export.test.cjs')
    if (Test-Path -LiteralPath 'tests/preset-library.test.cjs') {
        Invoke-Checked 'node' @('--test', '--test-isolation=none', 'tests/preset-library.test.cjs')
    }
    Invoke-Checked 'cl.exe' @('/nologo', '/std:c++20', '/EHsc', '/W4', '/WX', '/Isrc',
        'tests/preset_test.cpp', "/Fo:$work/preset_test.obj", "/Fe:$work/preset_test.exe")
    Invoke-Checked "$work/preset_test.exe" @()
    Invoke-Checked 'node' @('tests/profile-contract-fixture.cjs', "$work/exported.cfg")
    Invoke-Checked 'cl.exe' @('/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
        'tests/profile-contract.cpp', "/Fo:$work/profile-contract.obj", "/Fe:$work/profile-contract.exe")
    Invoke-Checked "$work/profile-contract.exe" @("$work/exported.cfg", 'acervo-case-hardened.cfg')
    Invoke-Checked 'cl.exe' @('/nologo', '/std:c++17', '/EHsc', '/utf-8', '/W4', '/WX',
        'tests/kits-test.cpp', "/Fo:$work/kits-test.obj", "/Fe:$work/kits-test.exe")
    Invoke-Checked "$work/kits-test.exe" @($work)
    Invoke-Checked $msbuild @('internal/vc.sln', '-nologo', '-m', '-v:minimal', '-t:Build',
        '-p:Configuration=Release', '-p:Platform=x64', "-p:AcervoVersion=$Version",
        "-p:AcervoObserveItems=$($ObserveItems.IsPresent)",
        "-p:WindowsTargetPlatformVersion=$sdkVersion", "-p:VCToolsVersion=$vcVersion",
        "-p:OutDir=$work/bin/", "-p:IntDir=$work/obj/", '-p:TrackFileAccess=false')
    Copy-Item -LiteralPath "$work/bin/CS.dll" -Destination $binaries
    Invoke-Checked 'cl.exe' @('/nologo', '/std:c++17', '/O2', '/MT', '/EHsc', 'internal/lab-loader.cpp',
        "/Fo:$work/lab-loader.obj", "/Fe:$binaries/lab-loader.exe")
    Invoke-Checked 'cl.exe' @('/nologo', '/std:c++17', '/O2', '/MT', '/EHsc', 'internal/acervo-launcher.cpp',
        "/Fo:$work/acervo-launcher.obj", "/Fe:$payload/Acervo-Laboratorio.exe",
        '/link', '/SUBSYSTEM:WINDOWS', 'shell32.lib', 'user32.lib')
}
finally { Pop-Location }

foreach ($path in @('docs', 'ui', 'acervo-case-hardened.cfg')) {
    Copy-Item -LiteralPath (Join-Path $source $path) -Destination $payload -Recurse
}
foreach ($path in @('assets', 'lab-attempt.ps1', 'lab-load.ps1', 'lab-preflight.ps1', 'lab-window.ps1', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $native $path) -Destination (Join-Path $payload 'internal') -Recurse
}

$sourceInventory = @(Get-Inventory $source)
$payloadInventory = @(Get-Inventory $payload)
$upstream = $null
if (Test-Path -LiteralPath "$nativeSource/.git") { $upstream = & git -C $nativeSource rev-parse HEAD }
$catalog = Get-Content -Raw -LiteralPath "$native/assets/catalog-source.json" | ConvertFrom-Json
$manifest = [ordered]@{
    version = $Version
    observe_items = $ObserveItems.IsPresent
    upstream_base = $upstream
    source_commit = $sourceCommit
    source_tree = $sourceTree
    msvc = $vcVersion
    windows_sdk = $sdkVersion
    node = (& node --version)
    python = (& python --version)
    runtime = 'Microsoft Visual C++ Redistributable x64'
    catalog = $catalog
    files = $payloadInventory
    source_files = $sourceInventory
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$payload/manifest.json" -Encoding UTF8
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$source/manifest.json" -Encoding UTF8

Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($payload, "$publication/Hy3x-$Version-windows-x64.zip")
[IO.Compression.ZipFile]::CreateFromDirectory($source, "$publication/Hy3x-$Version-source.zip")
foreach ($variant in @('windows-x64', 'source')) {
    $extracted = Join-Path $work "verify-$variant"
    [IO.Compression.ZipFile]::ExtractToDirectory("$publication/Hy3x-$Version-$variant.zip", $extracted)
    $expected = if ($variant -eq 'source') { $sourceInventory } else { $payloadInventory }
    Confirm-Inventory $extracted $expected
}
Copy-Item -LiteralPath "$payload/manifest.json" -Destination $publication
Move-Item -LiteralPath $publication -Destination $release
Write-Host "Versão validada em $release"
Write-Host "Arquivos de trabalho e símbolos em $work"
