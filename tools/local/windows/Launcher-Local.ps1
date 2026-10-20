param(
    [string]$PackageRoot = $PSScriptRoot,
    [switch]$CheckUI,
    [string]$PreviewPath = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
$script:playScript = Join-Path $PackageRoot 'Play-DemonsSouls.ps1'
if (!(Test-Path -LiteralPath $script:playScript)) { throw "Missing launch script: $script:playScript" }
$script:preferences = Join-Path $PackageRoot 'local-launcher-settings.json'
$script:heights = @(2160, 1800, 1620, 1512, 1440, 1296, 1152, 1080)
$saved = @{ hdr = $false; framegen = 2; render = 1440 }
if (Test-Path -LiteralPath $script:preferences) {
    try {
        $data = Get-Content -LiteralPath $script:preferences -Raw | ConvertFrom-Json
        if ($data.hdr -is [bool]) { $saved.hdr = $data.hdr }
        if ($data.framegen -in @(2, 3)) { $saved.framegen = [int]$data.framegen }
        if ($data.render -in $script:heights) { $saved.render = [int]$data.render }
    } catch { [System.Windows.Forms.MessageBox]::Show('Saved launcher settings could not be read. Using HDR Off, 2x and 1440.', 'Launcher') | Out-Null }
}

# Use the upstream launcher's Windows Forms approach; keep this local panel
# limited to the supported controls and route every launch through our wrapper.
$form = New-Object System.Windows.Forms.Form
$form.Text = "Demon's Souls - r19"
$form.ClientSize = New-Object System.Drawing.Size(570, 390)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.AutoScaleDimensions = New-Object System.Drawing.SizeF(96, 96)
$form.AutoScaleMode = 'Dpi'
$form.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$form.BackColor = [System.Drawing.Color]::FromArgb(246, 247, 249)
function Label($text, $x, $y, $w, $h) {
    $c = New-Object System.Windows.Forms.Label
    $c.Text = $text; $c.Location = New-Object System.Drawing.Point($x, $y)
    $c.Size = New-Object System.Drawing.Size($w, $h)
    $form.Controls.Add($c)
    return $c
}
$title = Label "Demon's Souls" 24 20 520 32
$title.Font = New-Object System.Drawing.Font('Segoe UI', 18, [System.Drawing.FontStyle]::Bold)
$subtitle = Label 'DLSS / Preset L / 4K output / Reflex enabled' 26 58 520 25
$subtitle.ForeColor = [System.Drawing.Color]::DimGray

$hdr = New-Object System.Windows.Forms.CheckBox
$hdr.Text = 'HDR'; $hdr.Checked = $saved.hdr
$hdr.Location = New-Object System.Drawing.Point(26, 101)
$hdr.Size = New-Object System.Drawing.Size(130, 27)
$form.Controls.Add($hdr)
$null = Label 'Requires HDR enabled in Windows display settings.' 176 103 365 42
$null = Label 'Frame generation' 26 158 150 27
$fg = New-Object System.Windows.Forms.ComboBox
$fg.DropDownStyle = 'DropDownList'; $fg.Location = New-Object System.Drawing.Point(176, 154)
$fg.Size = New-Object System.Drawing.Size(190, 30)
$fg.Items.AddRange([object[]]@('2x', '3x'))
$fg.SelectedIndex = $saved.framegen - 2
$form.Controls.Add($fg)
$null = Label 'Render height' 26 204 150 27
$height = New-Object System.Windows.Forms.ComboBox
$height.DropDownStyle = 'DropDownList'; $height.Location = New-Object System.Drawing.Point(176, 200)
$height.Size = New-Object System.Drawing.Size(235, 30)
$height.Items.AddRange([object[]]@('2160 (native 4K / DLAA)', '1800', '1620', '1512 (PS5 Pro)', '1440 (DLSS Quality)', '1296', '1152', '1080 (DLSS Performance)'))
$height.SelectedIndex = [array]::IndexOf($script:heights, $saved.render)
$form.Controls.Add($height)
$detail = Label '' 26 245 520 43
$detail.ForeColor = [System.Drawing.Color]::DimGray
$updateDetail = {
    $h = $script:heights[$height.SelectedIndex]
    $w = [int]([Math]::Round(($h * 16.0 / 9.0) / 8.0) * 8)
    $detail.Text = "Scene: ${w}x${h} -> 3840x2160 output.`r`nChanges apply on the next launch. Keep the game in Performance mode."
}
$height.Add_SelectedIndexChanged($updateDetail)
& $updateDetail

$status = Label 'Ready. Selections are saved when you press Play.' 26 296 520 30
$play = New-Object System.Windows.Forms.Button
$play.Text = 'Play'; $play.Location = New-Object System.Drawing.Point(421, 337)
$play.Size = New-Object System.Drawing.Size(122, 34)
$form.Controls.Add($play)
$logs = New-Object System.Windows.Forms.Button
$logs.Text = 'Open logs'; $logs.Location = New-Object System.Drawing.Point(26, 337)
$logs.Size = New-Object System.Drawing.Size(122, 34)
$form.Controls.Add($logs)
$logs.Add_Click({
    $dir = Join-Path $PackageRoot 'logs'
    if (Test-Path -LiteralPath $dir) { Start-Process explorer.exe -ArgumentList ('"' + $dir + '"') }
})
$tendency = New-Object System.Windows.Forms.Button
$tendency.Text = 'World Tendency'
$tendency.Location = New-Object System.Drawing.Point(163, 337)
$tendency.Size = New-Object System.Drawing.Size(178, 34)
$form.Controls.Add($tendency)
$tendency.Add_Click({
    try {
        . (Join-Path $PackageRoot 'World-Tendency.ps1')
        Show-WorldTendency -PackageRoot $PackageRoot
    } catch { [void][System.Windows.Forms.MessageBox]::Show($_.Exception.GetBaseException().Message, 'World Tendency') }
})
$script:launchProcess = $null
$script:launchError = ''
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 500
$timer.Add_Tick({
    if ($script:launchProcess -and $script:launchProcess.HasExited) {
        $timer.Stop()
        $script:launchProcess.WaitForExit()
        if ($script:launchProcess.ExitCode -ne 0) {
            $status.Text = 'Launch failed. See the error details.'
            $message = Get-Content -LiteralPath $script:launchError -Raw -ErrorAction SilentlyContinue
            if (!$message) { $message = 'The launch command failed. Check the latest launcher log.' }
            [System.Windows.Forms.MessageBox]::Show($message, 'Launch failed') | Out-Null
        } else { $status.Text = 'Launch command completed. Game output is in the logs folder.' }
        $script:launchProcess.Dispose(); $script:launchProcess = $null
        $play.Enabled = $true
    }
})
$play.Add_Click({
    try {
        if (Get-Process -Name kyty_emulator,kyty_shader_precompile -ErrorAction SilentlyContinue) {
            $status.Text = 'Close the game and finish precompilation before launching again.'
            return
        }
        $h = $script:heights[$height.SelectedIndex]
        $multiplier = $fg.SelectedIndex + 2
        @{ hdr = $hdr.Checked; framegen = $multiplier; render = $h } | ConvertTo-Json |
            Set-Content -LiteralPath $script:preferences -Encoding UTF8
        $pathLiteral = "'" + $script:playScript.Replace("'", "''") + "'"
        $hdrArg = if ($hdr.Checked) { ' -Hdr' } else { '' }
        $command = "`$ErrorActionPreference = 'Stop'; `$ProgressPreference = 'SilentlyContinue'; try { & $pathLiteral -FrameGenMultiplier $multiplier -RenderHeight $h$hdrArg; exit 0 } catch { [Console]::Error.WriteLine(`$_.ToString()); exit 1 }"
        $logDir = Join-Path $PackageRoot 'logs'
        New-Item -ItemType Directory -Path $logDir -Force | Out-Null
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
        $script:launchError = Join-Path $logDir "launcher-$stamp.err.log"
        $commandFile = Join-Path $logDir "launcher-$stamp.launch.ps1"
        Set-Content -LiteralPath $commandFile -Value $command -Encoding UTF8
        $shell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $script:launchProcess = Start-Process -FilePath $shell -ArgumentList @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $commandFile + '"')) -WorkingDirectory $PackageRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir "launcher-$stamp.out.log") -RedirectStandardError $script:launchError
        # Retain the native handle before the helper exits, so ExitCode stays available to the timer.
        $null = $script:launchProcess.Handle
        $status.Text = 'Launching...'; $play.Enabled = $false; $timer.Start()
    } catch { [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Launcher') | Out-Null }
})
$form.AcceptButton = $play
if ($CheckUI) {
    $form.CreateControl()
    if ($PreviewPath) {
        $form.StartPosition = 'Manual'
        $form.Location = New-Object System.Drawing.Point(-20000, -20000)
        $form.ShowInTaskbar = $false
        $form.Show()
        [System.Windows.Forms.Application]::DoEvents()
        $bitmap = New-Object System.Drawing.Bitmap($form.Width, $form.Height)
        $form.DrawToBitmap($bitmap, (New-Object System.Drawing.Rectangle(0, 0, $form.Width, $form.Height)))
        $bitmap.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        $form.Hide()
    }
    Write-Output "UI created: HDR=$($hdr.Checked), FG=$($fg.Text), render=$($height.Text)"
} else { [void]$form.ShowDialog() }
$timer.Dispose()
$form.Dispose()
