function Get-RunSettingsLabel([bool]$HdrRequested, [System.Collections.IDictionary]$Environment) {
    $color = if ($HdrRequested) { 'HDR' } else { 'SDR' }
    $fg = if ($Environment.Contains('KYTY_FRAMEGEN')) { 1 + [int]$Environment['KYTY_FRAMEGEN'] } else { 1 }
    $height = if ($Environment.Contains('KYTY_RENDER_HEIGHT')) { [int]$Environment['KYTY_RENDER_HEIGHT'] } else { 1440 }
    return "$color-FG${fg}x-RH$height"
}

function Get-WorldTendencyRunSnapshot([string]$PackageRoot) {
    $reader = Join-Path $PackageRoot 'WorldTendency.cs'
    if (!(Test-Path -LiteralPath $reader)) { return }
    try {
        if (-not ('KytyLocal.WorldTendency' -as [type])) { Add-Type -Path $reader }
        $title = Join-Path $PackageRoot '_SaveData\PPSA01341'
        foreach ($slot in @(Get-ChildItem -LiteralPath $title -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^SAVEDATA0PlayerProfile([0-9]|1[01])$' })) {
            try {
                $save = [KytyLocal.WorldTendency]::Read((Join-Path $slot.FullName 'USR-DATA'))
                $worlds = [ordered]@{}
                for ($i = 0; $i -lt 5; $i++) {
                    $index = [KytyLocal.WorldTendency]::Worlds[$i]
                    $worlds[[KytyLocal.WorldTendency]::Names[$i]] = @{
                        current = $save.Current[$index]; nextVisit = $save.Next[$index]
                        currentOn100Scale = ($save.Current[$index] + 200.0) / 4.0
                        nextVisitOn100Scale = ($save.Next[$index] + 200.0) / 4.0
                    }
                }
                @{ slot = $slot.Name; saveSHA256 = $save.Hash; worlds = $worlds
                   displayScale = '0 = Pure Black; 50 = Neutral; 100 = Pure White' }
            } catch { @{ slot = $slot.Name; readError = $_.Exception.GetBaseException().Message } }
        }
    } catch { @{ readError = $_.Exception.GetBaseException().Message } }
}

function Write-RunSettings {
    param([string]$Prefix, [string]$Exe, [string]$Game, [string]$GameVersion,
          [string[]]$Arguments, [System.Collections.IDictionary]$Environment,
          [bool]$HdrRequested, [string]$Config, [string]$PackageRoot)
    $effective = [ordered]@{}
    Get-ChildItem Env: | Where-Object { $_.Name -like 'KYTY_*' } | Sort-Object Name | ForEach-Object {
        $effective[$_.Name] = $_.Value
    }
    foreach ($key in $Environment.Keys) { $effective[$key] = $Environment[$key] }
    $buildPath = Join-Path $PackageRoot 'build-info.json'
    $build = if (Test-Path -LiteralPath $buildPath) { Get-Content -LiteralPath $buildPath -Raw | ConvertFrom-Json } else { $null }
    $record = [ordered]@{
        schemaVersion = 1; recordedAt = [DateTimeOffset]::Now.ToString('o')
        label = Get-RunSettingsLabel $HdrRequested $effective
        hdrRequested = $HdrRequested
        frameGenerationMultiplierRequested = 1 + [int]$effective['KYTY_FRAMEGEN']
        renderHeightRequested = [int]$effective['KYTY_RENDER_HEIGHT']
        srPresetRequested = $effective['KYTY_DLSS_PRESET']
        upscalerRequested = $effective['KYTY_UPSCALER']
        exePath = $Exe; exeSHA256 = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash
        build = $build; gamePath = $Game; gameVersion = $GameVersion
        configPath = $Config; configSHA256 = (Get-FileHash -LiteralPath $Config -Algorithm SHA256).Hash
        arguments = @($Arguments); environment = $effective
        worldTendencyAtLaunch = @(Get-WorldTendencyRunSnapshot $PackageRoot)
        stdoutLog = "$Prefix.out.log"; stderrLog = "$Prefix.err.log"
        status = 'starting'; processId = $null
        note = 'Requested startup configuration; runtime logs determine actual feature availability and fallback. In-game graphics mode is stored separately in the options save.'
    }
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$Prefix.settings.json" -Encoding UTF8
    @("Run settings: $($record.label)", "Recorded: $($record.recordedAt)",
      "Build: $($build.build)", "Executable SHA256: $($record.exeSHA256)",
      "Game: $GameVersion", "DLSS preset: $($record.srPresetRequested)",
      "Command: `"$Exe`" $($Arguments -join ' ')",
      "Full settings: $Prefix.settings.json", $record.note) |
        Set-Content -LiteralPath "$Prefix.settings.log" -Encoding UTF8
    return $record
}
