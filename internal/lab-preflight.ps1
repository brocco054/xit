$hy3xTestedGameBuild = '26598758'

function Confirm-LabGameBuild([string]$GameExecutable) {
    $gameDirectory = [IO.FileInfo]::new($GameExecutable).Directory.Parent.Parent
    $installation = $gameDirectory.Parent
    $manifestPath = Join-Path $installation.Parent.Parent.FullName 'appmanifest_730.acf'
    if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw 'Manifesto Steam do CS2 ausente. Nao foi possivel confirmar a build instalada.'
    }
    $steamManifest = Get-Content -LiteralPath $manifestPath -Raw
    $builds = [regex]::Matches($steamManifest, '(?im)^\s*"buildid"\s*"([0-9]+)"\s*$')
    $names = [regex]::Matches($steamManifest, '(?im)^\s*"installdir"\s*"([^"]+)"\s*$')
    if ($builds.Count -ne 1 -or $names.Count -ne 1 -or $names[0].Groups[1].Value -ne $installation.Name) {
        throw 'Manifesto Steam do CS2 invalido ou de outra instalacao.'
    }
    $installedBuild = $builds[0].Groups[1].Value
    if ($installedBuild -ne $hy3xTestedGameBuild) {
        throw "Build instalada do CS2: $installedBuild. Build testada pelo Hy3x: $hy3xTestedGameBuild."
    }
    return $installedBuild
}

function Confirm-LabModule([string]$InternalDirectory, [IO.FileInfo]$ModuleFile) {
    $manifestPath = Join-Path (Split-Path -Parent $InternalDirectory) 'manifest.json'
    if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        if (Test-Path -LiteralPath (Join-Path $InternalDirectory 'vc.sln') -PathType Leaf) {
            return 'Pasta de desenvolvimento. Manifesto do pacote indisponivel.'
        }
        throw 'manifest.json ausente. Extraia o ZIP inteiro em uma pasta nova.'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $entry = @($manifest.files | Where-Object path -eq 'internal/x64/Release/CS.dll')
    if ($entry.Count -ne 1 -or $entry[0].sha256 -notmatch '\A[0-9a-fA-F]{64}\z') {
        throw 'O manifesto do pacote nao informa um SHA-256 valido para CS.dll.'
    }
    if ((Get-FileHash -LiteralPath $ModuleFile.FullName -Algorithm SHA256).Hash -ne $entry[0].sha256) {
        throw 'A CS.dll diverge do manifesto. Extraia o ZIP inteiro em uma pasta nova.'
    }
    return 'SHA-256 da CS.dll conferido.'
}
