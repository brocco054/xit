[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?$')]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
$release = Join-Path $dist "Hy3x-Acervo-Companion-$Version"
if (Test-Path -LiteralPath $release) { throw "Versão já publicada: $release" }

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program falhou (código $LASTEXITCODE)." }
}

$node = Get-Command node -ErrorAction Stop
Invoke-Checked $node.Source @('--test', '--test-isolation=none', 'tests/profile-export.test.cjs')
if (Test-Path -LiteralPath (Join-Path $root 'tests/preset-library.test.cjs')) {
    Invoke-Checked $node.Source @('--test', '--test-isolation=none', 'tests/preset-library.test.cjs')
}

$staging = Join-Path $dist ('.companion-' + [guid]::NewGuid().ToString('N'))
$payload = Join-Path $staging 'package'
$verify = Join-Path $staging 'verify'
$null = New-Item -ItemType Directory -Path $payload -Force
$null = New-Item -ItemType Directory -Path $verify -Force
try {
    Copy-Item -LiteralPath (Join-Path $root 'ui') -Destination $payload -Recurse
    Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $payload
    Copy-Item -LiteralPath (Join-Path $root 'docs/companion-mode.md') -Destination $payload
    if (Test-Path -LiteralPath (Join-Path $root 'internal/LICENSE')) {
        Copy-Item -LiteralPath (Join-Path $root 'internal/LICENSE') -Destination (Join-Path $payload 'LICENSE')
    }
    $forbidden = @(Get-ChildItem -LiteralPath $payload -File -Recurse | Where-Object {
        $_.Name -match '^(CS\.dll|lab-loader\.exe|Acervo-Laboratorio\.exe)$' -or $_.Extension -in '.cfg', '.sys', '.exe'
    })
    if ($forbidden.Count) { throw 'O pacote companion contém um binário de laboratório ou configuração nativa.' }

    $files = @(Get-ChildItem -LiteralPath $payload -File -Recurse | ForEach-Object {
        [pscustomobject][ordered]@{
            path = $_.FullName.Substring($payload.Length + 1).Replace('\', '/')
            bytes = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } | Sort-Object path)
    $manifest = [ordered]@{
        product = 'Hy3x Acervo Companion'
        version = $Version
        runtime = 'Browser (no game-process integration)'
        files = $files
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $payload 'manifest.json') -Encoding UTF8

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = Join-Path $staging "Hy3x-Acervo-Companion-$Version.zip"
    [IO.Compression.ZipFile]::CreateFromDirectory($payload, $zip)
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $verify)
    foreach ($file in $files) {
        $path = Join-Path $verify ($file.path.Replace('/', [IO.Path]::DirectorySeparatorChar))
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Arquivo ausente no ZIP: $($file.path)" }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256) {
            throw "Hash divergente no ZIP: $($file.path)"
        }
    }
    if (!(Test-Path -LiteralPath (Join-Path $verify 'manifest.json') -PathType Leaf)) { throw 'Manifesto ausente no pacote.' }
    $null = New-Item -ItemType Directory -Path $release -Force
    Move-Item -LiteralPath $zip -Destination (Join-Path $release (Split-Path -Leaf $zip))
    Copy-Item -LiteralPath (Join-Path $verify 'manifest.json') -Destination $release
    Write-Host "Companion validado em $release"
}
finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}