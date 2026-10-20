# Dot-source from Launcher-Local.ps1. Loading and previewing this panel never writes a save.
if (-not ('KytyLocal.WorldTendency' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'WorldTendency.cs')
}

function Show-WorldTendency {
    param([string]$PackageRoot, [switch]$CheckUI, [string]$PreviewPath = '')
    Add-Type -AssemblyName System.Windows.Forms, System.Drawing
    $dialog = New-Object System.Windows.Forms.Form
    $dialog.Text = "Demon's Souls - World Tendency"
    $dialog.ClientSize = New-Object System.Drawing.Size(710, 510)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.AutoScaleDimensions = New-Object System.Drawing.SizeF(96, 96)
    $dialog.AutoScaleMode = 'Dpi'
    $dialog.Font = New-Object System.Drawing.Font('Segoe UI', 10)
    $dialog.BackColor = [System.Drawing.Color]::FromArgb(246, 247, 249)
    $state = @{ snapshot = $null; lastEdit = $null }
    $choices = @()
    $readouts = @()
    $slots = @(Get-ChildItem -LiteralPath (Join-Path $PackageRoot '_SaveData\PPSA01341') -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^SAVEDATA0PlayerProfile([0-9]|1[01])$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'USR-DATA')) } |
        Sort-Object { [int]($_.Name -replace '^SAVEDATA0PlayerProfile', '') })
    function Add-TendencyLabel($text, $x, $y, $w, $h) {
        $label = New-Object System.Windows.Forms.Label
        $label.Text = $text
        $label.Location = New-Object System.Drawing.Point($x, $y)
        $label.Size = New-Object System.Drawing.Size($w, $h)
        $dialog.Controls.Add($label)
        return $label
    }
    function Format-Tendency($value) {
        return ('{0:0.##}' -f (($value + 200.0) / 4.0))
    }
    $heading = Add-TendencyLabel 'World Tendency' 22 18 455 35
    $heading.Font = New-Object System.Drawing.Font('Segoe UI', 17, [System.Drawing.FontStyle]::Bold)
    $back = New-Object System.Windows.Forms.Button
    $back.Text = 'Back to launcher'
    $back.Location = New-Object System.Drawing.Point(503, 20)
    $back.Size = New-Object System.Drawing.Size(180, 32)
    $back.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($back)
    $dialog.CancelButton = $back
    $null = Add-TendencyLabel 'Apply once while the game is closed. Changes take effect when you load this character.' 24 60 660 45
    $null = Add-TendencyLabel 'Character save' 24 111 130 25
    $profile = New-Object System.Windows.Forms.ComboBox
    $profile.DropDownStyle = 'DropDownList'
    $profile.Location = New-Object System.Drawing.Point(160, 107)
    $profile.Size = New-Object System.Drawing.Size(350, 30)
    foreach ($slot in $slots) { [void]$profile.Items.Add($slot.Name) }
    $dialog.Controls.Add($profile)
    $null = Add-TendencyLabel 'World' 24 155 220 25
    $null = Add-TendencyLabel 'Now / next (0 to 100)' 250 155 237 25
    $null = Add-TendencyLabel 'Set to' 493 155 190 25
    for ($i = 0; $i -lt 5; $i++) {
        $y = 188 + 37 * $i
        $null = Add-TendencyLabel ([KytyLocal.WorldTendency]::Names[$i]) 24 $y 220 29
        $readout = Add-TendencyLabel '' 250 $y 237 29
        $readout.ForeColor = [System.Drawing.Color]::DimGray
        $readouts += $readout
        $choice = New-Object System.Windows.Forms.ComboBox
        $choice.DropDownStyle = 'DropDownList'
        $choice.Location = New-Object System.Drawing.Point(493, ($y - 3))
        $choice.Size = New-Object System.Drawing.Size(190, 30)
        [void]$choice.Items.Add('Keep current')
        for ($value = 0; $value -le 100; $value += 10) {
            $text = '{0:0}' -f $value
            if ($value -eq 0) { $text += ' (Pure Black)' }
            if ($value -eq 50) { $text += ' (Neutral)' }
            if ($value -eq 100) { $text += ' (Pure White)' }
            [void]$choice.Items.Add($text)
        }
        $choice.SelectedIndex = 0
        $dialog.Controls.Add($choice)
        $choices += $choice
    }
    $null = Add-TendencyLabel 'Creates a verified backup. Character Tendency and progression flags are preserved.' 24 379 660 28
    $notice = Add-TendencyLabel 'Choose the worlds to change, then Apply.' 24 410 660 45
    $notice.ForeColor = [System.Drawing.Color]::DimGray
    $apply = New-Object System.Windows.Forms.Button
    $apply.Text = 'Apply'
    $apply.Location = New-Object System.Drawing.Point(563, 465)
    $apply.Size = New-Object System.Drawing.Size(120, 32)
    $dialog.Controls.Add($apply)
    $undo = New-Object System.Windows.Forms.Button
    $undo.Text = 'Undo this edit'
    $undo.Enabled = $false
    $undo.Location = New-Object System.Drawing.Point(392, 465)
    $undo.Size = New-Object System.Drawing.Size(155, 32)
    $dialog.Controls.Add($undo)
    $backups = New-Object System.Windows.Forms.Button
    $backups.Text = 'Open backups'
    $backups.Location = New-Object System.Drawing.Point(24, 465)
    $backups.Size = New-Object System.Drawing.Size(145, 32)
    $dialog.Controls.Add($backups)
    $reload = New-Object System.Windows.Forms.Button
    $reload.Text = 'Reload save'
    $reload.Location = New-Object System.Drawing.Point(184, 465)
    $reload.Size = New-Object System.Drawing.Size(145, 32)
    $dialog.Controls.Add($reload)
    $refresh = {
        $state.snapshot = $null
        $apply.Enabled = $false
        try {
            if ($profile.SelectedIndex -lt 0) { throw 'No supported character saves found in this build.' }
            $path = Join-Path $slots[$profile.SelectedIndex].FullName 'USR-DATA'
            $state.snapshot = [KytyLocal.WorldTendency]::Read($path)
            for ($i = 0; $i -lt 5; $i++) {
                $world = [KytyLocal.WorldTendency]::Worlds[$i]
                $readouts[$i].Text = (Format-Tendency $state.snapshot.Current[$world]) + ' / ' +
                    (Format-Tendency $state.snapshot.Next[$world])
                $choices[$i].SelectedIndex = 0
            }
            $apply.Enabled = $true
            $notice.Text = 'Choose the worlds to change, then Apply.'
        } catch {
            foreach ($readout in $readouts) { $readout.Text = 'Unavailable' }
            $notice.Text = $_.Exception.GetBaseException().Message
        }
    }
    $profile.Add_SelectedIndexChanged({
        $state.lastEdit = $null
        $undo.Enabled = $false
        & $refresh
    })
    $reload.Add_Click({ & $refresh })
    $backups.Add_Click({
        $path = Join-Path $PackageRoot 'backups\world-tendency'
        if (Test-Path -LiteralPath $path) { Start-Process explorer.exe -ArgumentList ('"' + $path + '"') }
        else { $notice.Text = 'No tendency backups yet. A backup is created before your first change.' }
    })
    $apply.Add_Click({
        try {
            if (!$state.snapshot) { throw 'Reload the save first.' }
            $changes = New-Object 'System.Collections.Generic.Dictionary[int,int]'
            for ($i = 0; $i -lt 5; $i++) {
                if ($choices[$i].SelectedIndex -gt 0) {
                    $changes.Add([KytyLocal.WorldTendency]::Worlds[$i], ($choices[$i].SelectedIndex - 1) * 10)
                }
            }
            if ($changes.Count -eq 0) { $notice.Text = 'Select a tendency for at least one world.'; return }
            $result = [KytyLocal.WorldTendency]::Apply($state.snapshot.Path, $changes, $state.snapshot.Hash)
            & $refresh
            if ($result.Changed) {
                $state.lastEdit = $result
                $undo.Enabled = $true
                $notice.Text = 'Applied and backed up. Load this character to use the new tendency.'
            } else { $notice.Text = 'The selected worlds already have those values. No save changes needed.' }
        } catch { [void][System.Windows.Forms.MessageBox]::Show($_.Exception.GetBaseException().Message, 'World Tendency') }
    })
    $undo.Add_Click({
        try {
            [void][KytyLocal.WorldTendency]::Undo($state.lastEdit)
            $state.lastEdit = $null
            $undo.Enabled = $false
            & $refresh
            $notice.Text = 'Original tendency restored. The undone edit also has a backup.'
        } catch { [void][System.Windows.Forms.MessageBox]::Show($_.Exception.GetBaseException().Message, 'World Tendency') }
    })
    if ($slots.Count -gt 0) { $profile.SelectedIndex = 0 } else { & $refresh }
    try {
        if ($CheckUI) {
            if ($PreviewPath) {
                $dialog.StartPosition = 'Manual'
                $dialog.Location = New-Object System.Drawing.Point(-20000, -20000)
                $dialog.ShowInTaskbar = $false
                $dialog.Show()
                [System.Windows.Forms.Application]::DoEvents()
                $bitmap = New-Object System.Drawing.Bitmap($dialog.Width, $dialog.Height)
                $dialog.DrawToBitmap($bitmap, (New-Object System.Drawing.Rectangle(0, 0, $dialog.Width, $dialog.Height)))
                $bitmap.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
                $bitmap.Dispose()
                $dialog.Hide()
            }
            Write-Output "Tendency UI: $($slots.Count) slots; readable=$($null -ne $state.snapshot)"
        } else { [void]$dialog.ShowDialog() }
    } finally { $dialog.Dispose() }
}
