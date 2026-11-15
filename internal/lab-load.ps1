param([switch]$CheckOnly, [switch]$MeasureFrames, [switch]$StatusEvents)

$ErrorActionPreference = 'Stop'


$diagnostic = @{
    AttemptId = [guid]::NewGuid().ToString('N')
    Stage = 'rede'
    ProcessId = $null
    LogPath = $null
    ReportedFailure = $false
}

function Write-LabStatus(
    [string]$Stage, [string]$Result, [string]$Message,
    [switch]$HostOnly, [int]$ExitCode
) {
    $diagnostic.Stage = $Stage
    $failure = $Result -in 'falhou', 'bloqueado', 'indeterminado'
    if ($failure) { $diagnostic.ReportedFailure = $true }
    if ($StatusEvents) {
        Write-Information -InformationAction Continue -Tags 'Hy3xStatus' -MessageData ([pscustomobject]@{
            AttemptId = $diagnostic.AttemptId
            Timestamp = [DateTimeOffset]::UtcNow.ToString('o')
            Stage = $Stage
            Result = $Result
            Message = $Message
            ProcessId = $diagnostic.ProcessId
            LogPath = $diagnostic.LogPath
            ExitCode = if ($PSBoundParameters.ContainsKey('ExitCode')) { $ExitCode }
        })
    } elseif ($HostOnly) {
        Write-Host $Message
    } elseif (!$failure) {
        Write-Output $Message
    }
}

try {
    Write-LabStatus 'rede' 'em_andamento' 'Pré-checagem de rede do laboratório.'
    $externalAdapters = [Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() |
        Where-Object {
            $_.OperationalStatus -eq 'Up' -and
            $_.NetworkInterfaceType -ne 'Loopback' -and
            @($_.GetIPProperties().UnicastAddresses | ForEach-Object Address | Where-Object {
                -not [Net.IPAddress]::IsLoopback($_) -and
                -not $_.IsIPv6LinkLocal -and
                $_.ToString() -notlike '169.254.*'
            }).Count -gt 0
        }

    if ($externalAdapters) {
        throw ('O laboratório deve estar sem rede. Interfaces ativas: ' + ($externalAdapters.Name -join ', '))
    }

    if ($CheckOnly) {
        Write-LabStatus 'rede' 'verificado' 'Nenhuma interface externa com endereço IP foi detectada nesta checagem.'
        return
    }

    Write-LabStatus 'sessao' 'em_andamento' 'Verificando a sessão local do CS2.'
    $game = @(Get-Process -Name cs2 -ErrorAction SilentlyContinue)
    if ($game.Count -ne 1 -or !$game[0].Path) {
        throw 'Abra uma única instância do CS2 no mapa local antes de carregar o módulo.'
    }

    $diagnostic.ProcessId = $game[0].Id
    $commandLine = (Get-CimInstance -ClassName Win32_Process -Filter "ProcessId = $($game[0].Id)").CommandLine
    if (!$commandLine -or $commandLine -notmatch '(?:^|\s)(?:"-insecure"|-insecure)(?=\s|$)') {
        throw 'O iniciador não confirmou a opção -insecure nesta instância do CS2. Reinicie a sessão de laboratório.'
    }

    . (Join-Path $PSScriptRoot 'lab-preflight.ps1')
    $diagnostic.Stage = 'build'
    $testedBuild = Confirm-LabGameBuild $game[0].Path
    Write-LabStatus 'build' 'verificado' "Build do CS2 confirmada: $testedBuild."
    $diagnostic.Stage = 'pacote'
    $module = Get-Item -LiteralPath (Join-Path $PSScriptRoot 'x64/Release/CS.dll')
    $moduleCheck = Confirm-LabModule $PSScriptRoot $module
    Write-LabStatus 'pacote' 'concluido' $moduleCheck
    $loader = Get-Item -LiteralPath (Join-Path $PSScriptRoot 'x64/Release/lab-loader.exe')
    foreach ($runtime in @('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$runtime") -PathType Leaf)) {
            throw "Dependência ausente: $runtime. Instale o Microsoft Visual C++ Redistributable x64."
        }
    }

    $diagnostics = Join-Path $env:LOCALAPPDATA 'Hy3x'
    $sessionLog = Join-Path $diagnostics "sessao-$($game[0].Id).log"
    $diagnostic.LogPath = $sessionLog
    $measurementFlag = Join-Path $diagnostics "medir-$($game[0].Id).flag"
    New-Item -ItemType Directory -Path $diagnostics -Force | Out-Null
    . (Join-Path $PSScriptRoot 'lab-attempt.ps1')

    $diagnostic.Stage = 'reserva'
    $attempt = Invoke-LabAttempt -MarkerDirectory $diagnostics -ProcessId $game[0].Id -ProcessStartedAt $game[0].StartTime -RunWorker {
        try {
            if ($MeasureFrames) {
                New-Item -ItemType File -Path $measurementFlag -Force | Out-Null
            }

            $startedAt = [DateTimeOffset]::UtcNow
            Write-LabStatus 'carga' 'em_andamento' 'Carregando módulo.' -HostOnly
            $workerExitCode = & {
                $ErrorActionPreference = 'Continue'
                $PSNativeCommandUseErrorActionPreference = $false
                $global:LASTEXITCODE = $null
                & $loader.FullName $game[0].Id $module.FullName 2>&1 | ForEach-Object { Write-Host $_ }
                $global:LASTEXITCODE
            }
            if ($workerExitCode -ne 0) { return $workerExitCode }

            Write-LabStatus 'primeiro_quadro' 'em_andamento' 'DLL presente. Aguardando o primeiro quadro do painel.' -HostOnly
            $deadline = [DateTimeOffset]::UtcNow.AddSeconds(15)
            do {
                if (Test-Path -LiteralPath $sessionLog -PathType Leaf) {
                    foreach ($line in Get-Content -LiteralPath $sessionLog -Tail 30) {
                        if ($line -match '^(\S+) (\S+)(?: (.*))?$') {
                            $eventTime = [DateTimeOffset]::MinValue
                            if (![DateTimeOffset]::TryParse($Matches[1], [ref]$eventTime) -or
                                $eventTime -lt $startedAt.AddMilliseconds(-1)) { continue }
                            switch ($Matches[2]) {
                                'first_frame' { return 0 }
                                'startup_error' { throw "Falha na inicialização: $($Matches[3]). Log: $sessionLog" }
                            }
                        }
                    }
                }
                if ($game[0].HasExited) {
                    throw "O jogo encerrou antes do primeiro quadro. Log: $sessionLog"
                }
                Start-Sleep -Milliseconds 250
            } while ([DateTimeOffset]::UtcNow -lt $deadline)
            Write-Host "Primeiro quadro ainda indisponível. Log: $sessionLog"
            return 3
        } finally {
            if ($MeasureFrames) { Remove-Item -LiteralPath $measurementFlag -Force -ErrorAction SilentlyContinue }
        }
    }

    switch ($attempt.State) {
        'blocked' {
            $message = 'Já existe uma carga em andamento ou indeterminada para esta instância. Se a tentativa anterior expirou, reinicie o CS2.'
            Write-LabStatus 'reserva' 'bloqueado' $message
            throw $message
        }
        'indeterminate' {
            $message = "Carga indeterminada. Reinicie o CS2 antes de tentar novamente. Log: $sessionLog"
            Write-LabStatus $diagnostic.Stage 'indeterminado' $message -ExitCode $attempt.ExitCode
            throw $message
        }
        'finished' {
            if ($attempt.ExitCode -ne 0) {
                $message = "O carregador retornou $($attempt.ExitCode)."
                Write-LabStatus 'carga' 'falhou' $message -ExitCode $attempt.ExitCode
                throw $message
            }
            Write-LabStatus 'primeiro_quadro' 'pronto' "Painel pronto. Use Delete para abrir ou fechar. Log: $sessionLog" -ExitCode 0
        }
    }
} catch {
    if (!$diagnostic.ReportedFailure) {
        Write-LabStatus $diagnostic.Stage 'falhou' $_.Exception.Message
    }
    throw
}
