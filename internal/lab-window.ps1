$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()

$form = New-Object Windows.Forms.Form
$form.Text = 'Hy3x | Laboratório'
$form.ClientSize = [Drawing.Size]::new(660, 410)
$form.MinimumSize = [Drawing.Size]::new(595, 380)
$form.StartPosition = 'CenterScreen'
$form.AutoScaleMode = 'Dpi'
$form.Font = [Drawing.Font]::new('Segoe UI', 10)
$form.BackColor = [Drawing.Color]::FromArgb(24, 23, 29)
$form.ForeColor = [Drawing.Color]::WhiteSmoke

$layout = New-Object Windows.Forms.TableLayoutPanel
$layout.Dock = 'Fill'
$layout.Padding = [Windows.Forms.Padding]::new(18)
$layout.ColumnCount = 1
$layout.RowCount = 4
foreach ($height in 36, 52) {
    $null = $layout.RowStyles.Add([Windows.Forms.RowStyle]::new('Absolute', $height))
}
$null = $layout.RowStyles.Add([Windows.Forms.RowStyle]::new('Percent', 100))
$null = $layout.RowStyles.Add([Windows.Forms.RowStyle]::new('Absolute', 52))
$form.Controls.Add($layout)

$title = New-Object Windows.Forms.Label
$title.Text = 'Hy3x'
$title.Font = [Drawing.Font]::new('Segoe UI Semibold', 20)
$title.Dock = 'Fill'
$layout.Controls.Add($title, 0, 0)

$status = New-Object Windows.Forms.Label
$status.Dock = 'Fill'
$status.TextAlign = 'MiddleLeft'
$status.Text = 'Abra um mapa local no CS2. Em seguida, clique em Carregar painel.'
$layout.Controls.Add($status, 0, 1)

$details = New-Object Windows.Forms.TextBox
$details.Multiline = $true
$details.ReadOnly = $true
$details.ScrollBars = 'Vertical'
$details.Dock = 'Fill'
$details.BackColor = [Drawing.Color]::FromArgb(35, 33, 43)
$details.ForeColor = [Drawing.Color]::WhiteSmoke
$layout.Controls.Add($details, 0, 2)

$actions = New-Object Windows.Forms.FlowLayoutPanel
$actions.Dock = 'Fill'
$actions.WrapContents = $false
$actions.Padding = [Windows.Forms.Padding]::new(0, 8, 0, 0)
$layout.Controls.Add($actions, 0, 3)

function Add-Action([string]$Label) {
    $button = New-Object Windows.Forms.Button
    $button.Text = $Label
    $button.AutoSize = $true
    $button.Padding = [Windows.Forms.Padding]::new(9, 3, 9, 3)
    $button.ForeColor = [Drawing.Color]::Black
    $button.UseVisualStyleBackColor = $true
    $actions.Controls.Add($button)
    return $button
}

$load = Add-Action 'Carregar painel'
$verify = Add-Action 'Verificar rede'
$copy = Add-Action 'Copiar diagnóstico'
$copy.Enabled = $false
$form.AcceptButton = $load

$session = @{ Worker = $null; Invocation = $null; Queue = $null; Latest = $null }
$stepNames = @{
    rede = 'Rede'
    sessao = 'Sessão do CS2'
    build = 'Build do jogo'
    pacote = 'Pacote'
    reserva = 'Reserva da sessão'
    carga = 'Carga do módulo'
    primeiro_quadro = 'Primeiro quadro'
}
$scriptPath = Join-Path $PSScriptRoot 'lab-load.ps1'
$timer = New-Object Windows.Forms.Timer
$timer.Interval = 100

$task = {
    param($Path, $CheckOnly, $Queue)
    $ErrorActionPreference = 'Stop'
    try {
        & $Path -CheckOnly:$CheckOnly -StatusEvents *>&1 | ForEach-Object {
            if ($_ -is [Management.Automation.InformationRecord] -and $_.Tags -contains 'Hy3xStatus') {
                $Queue.Enqueue($_.MessageData)
            } else {
                $Queue.Enqueue([string]$_)
            }
        }
    } catch {
        $Queue.Enqueue($_.Exception.Message)
        throw
    }
}

function Start-Session([bool]$CheckOnly) {
    $details.Clear()
    $status.Text = 'Iniciando a verificação…'
    $session.Latest = $null
    $session.Queue = New-Object 'System.Collections.Concurrent.ConcurrentQueue[object]'
    $session.Worker = [powershell]::Create()
    try {
        $null = $session.Worker.AddScript($task.ToString()).AddArgument($scriptPath).AddArgument($CheckOnly).AddArgument($session.Queue)
        $session.Invocation = $session.Worker.BeginInvoke()
        $load.Enabled = $false
        $verify.Enabled = $false
        $copy.Enabled = $false
        $timer.Start()
    } catch {
        $session.Worker.Dispose()
        $session.Worker = $null
        $status.Text = 'Falha ao iniciar a verificação.'
        $details.Text = $_.Exception.Message
        $copy.Enabled = $true
    }
}

function Read-Diagnostics {
    $item = $null
    while ($session.Queue.TryDequeue([ref]$item)) {
        if ($item -is [string]) {
            if ($item -and $item -ne $session.Latest.Message) {
                $details.AppendText($item + [Environment]::NewLine)
                $copy.Enabled = $true
            }
        } elseif ($item) {
            if (!$session.Latest) {
                $details.AppendText("Tentativa $($item.AttemptId)" + [Environment]::NewLine)
            }
            $session.Latest = $item
            $step = $stepNames[$item.Stage]
            $details.AppendText("[$step] $($item.Message)" + [Environment]::NewLine)
            $status.Text = $item.Message
            $copy.Enabled = $true
        }
    }
}

$timer.Add_Tick({
    Read-Diagnostics
    if ($session.Invocation.IsCompleted) {
        $timer.Stop()
        try {
            $null = $session.Worker.EndInvoke($session.Invocation)
            Read-Diagnostics
        } catch {
            Read-Diagnostics
            if ($session.Latest.Result -notin 'falhou', 'bloqueado', 'indeterminado') {
                $status.Text = 'A sessão não foi concluída. Veja o diagnóstico abaixo.'
            }
        } finally {
            $session.Worker.Dispose()
            $session.Worker = $null
            $load.Enabled = $true
            $verify.Enabled = $true
        }
    }
})

$verify.Add_Click({ Start-Session $true })
$load.Add_Click({ Start-Session $false })
$copy.Add_Click({
    try {
        [Windows.Forms.Clipboard]::SetText($details.Text)
        $status.Text = 'Diagnóstico copiado.'
    } catch {
        $status.Text = 'Selecione o diagnóstico e pressione Ctrl+C.'
    }
})
$form.Add_FormClosing({
    if ($session.Worker) {
        $_.Cancel = $true
        $status.Text = 'Aguarde a tentativa atual terminar.'
    }
})

try { $null = $form.ShowDialog() } finally {
    $timer.Dispose()
    $form.Dispose()
}
