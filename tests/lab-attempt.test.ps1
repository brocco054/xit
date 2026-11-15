$ErrorActionPreference = 'Stop'
$implementation = Join-Path $PSScriptRoot '../internal/lab-attempt.ps1'
. $implementation

$directory = [IO.Directory]::CreateDirectory((Join-Path ([IO.Path]::GetTempPath()) ('hy3x-attempt-' + [guid]::NewGuid().ToString('N')))).FullName
$calls = Join-Path $directory 'calls.log'
$startedAt = [datetime]::UtcNow
$attempt = @{ MarkerDirectory = $directory; ProcessId = 42; ProcessStartedAt = $startedAt }

try {
    $first = Invoke-LabAttempt @attempt -RunWorker {
        Add-Content -LiteralPath $calls -Value 'first'
        $concurrent = Invoke-LabAttempt @attempt -RunWorker { throw 'Concurrent worker was called.' }
        if ($concurrent.State -ne 'blocked') { throw 'Concurrent request was not blocked.' }
        return 3
    }
    if ($first.State -ne 'indeterminate' -or $first.ExitCode -ne 3) { throw 'Timeout state was lost.' }

    # Reload the code to model reopening the launcher. The reservation lives on disk.
    . $implementation
    $retry = Invoke-LabAttempt @attempt -RunWorker { throw 'Worker ran again for the same process instance.' }
    if ($retry.State -ne 'blocked' -or @(Get-Content -LiteralPath $calls).Count -ne 1) {
        throw 'Retry after timeout was not blocked.'
    }

    # Windows can reuse a PID after a process exits.
    $attempt.ProcessStartedAt = $startedAt.AddSeconds(1)
    $next = Invoke-LabAttempt @attempt -RunWorker {
        Add-Content -LiteralPath $calls -Value 'new instance'
        return 0
    }
    if ($next.State -ne 'finished' -or $next.ExitCode -ne 0 -or @(Get-Content -LiteralPath $calls).Count -ne 2) {
        throw 'The new process instance did not run.'
    }
    'PASS: active attempt blocked, timeout prevents retry, new instance accepted.'
} finally {
    Get-ChildItem -LiteralPath $directory -File | Remove-Item -Force
    Remove-Item -LiteralPath $directory
}
