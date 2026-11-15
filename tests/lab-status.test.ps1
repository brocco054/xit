$ErrorActionPreference = 'Stop'
$loader = Join-Path $PSScriptRoot '../internal/lab-load.ps1'
$output = @(try { & $loader -CheckOnly -StatusEvents 6>&1 } catch { $_ })
$failure = @($output | Where-Object { $_ -is [Management.Automation.ErrorRecord] })
$events = @($output | Where-Object { $_.Tags -contains 'Hy3xStatus' } | ForEach-Object MessageData)
if ($events.Count -ne 2) { throw 'Expected two status events for a network check.' }
$start, $end = $events
if ($start.Stage -ne 'rede' -or $start.Result -ne 'em_andamento' -or
    $end.Stage -ne 'rede' -or $end.AttemptId -ne $start.AttemptId -or
    $end.AttemptId -notmatch '\A[0-9a-f]{32}\z' -or $end.ProcessId -or $end.LogPath -or $null -ne $end.ExitCode) {
    throw 'Network check status did not describe one attempt without a game process.'
}
if (($failure.Count -eq 1 -and $end.Result -ne 'falhou') -or
    ($failure.Count -eq 0 -and $end.Result -ne 'verificado') -or $failure.Count -gt 1) {
    throw 'Network check status did not match the script outcome.'
}
'PASS: network check stage, attempt ID and terminal result.'
