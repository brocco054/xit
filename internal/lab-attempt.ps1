function Invoke-LabAttempt {
    param(
        [Parameter(Mandatory)][string]$MarkerDirectory,
        [Parameter(Mandatory)][int]$ProcessId,
        [Parameter(Mandatory)][datetime]$ProcessStartedAt,
        [Parameter(Mandatory)][scriptblock]$RunWorker
    )

    $instance = '{0}-{1}' -f $ProcessId, $ProcessStartedAt.ToUniversalTime().Ticks
    $marker = Join-Path $MarkerDirectory "load-$instance.pending"
    try {
        $reservation = [IO.File]::Open($marker, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    } catch [IO.IOException] {
        if (![IO.File]::Exists($marker)) { throw }
        return [pscustomobject]@{ State = 'blocked'; ExitCode = $null }
    }

    $releaseMarker = $false
    try {
        $reservation.Dispose()
        $code = & $RunWorker
        if ($code -isnot [int]) { throw 'O carregador deve retornar exatamente um codigo inteiro.' }
        if ($code -eq 3) {
            return [pscustomobject]@{ State = 'indeterminate'; ExitCode = $code }
        }

        $releaseMarker = $true
        return [pscustomobject]@{ State = 'finished'; ExitCode = $code }
    } finally {
        if ($releaseMarker) { [IO.File]::Delete($marker) }
    }
}
