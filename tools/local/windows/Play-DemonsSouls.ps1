param(
    [switch]$Hdr,
    [ValidateSet(2, 3)][int]$FrameGenMultiplier = 2,
    [ValidateSet(1080, 1152, 1296, 1440, 1512, 1620, 1800, 2160)][int]$RenderHeight = 1440,
    [switch]$DryRun,
    [switch]$Follow,
    [switch]$HudlessFrameGen,
    [switch]$SrTiming,
    [switch]$DisableFreshMemory
)
$ErrorActionPreference = 'Stop'
if (!$DryRun -and (Get-Process -Name kyty_emulator,kyty_shader_precompile -ErrorAction SilentlyContinue)) {
    throw 'Close the emulator and finish shader precompilation before launching this comparison.'
}
$runtimeSettings = 'KYTY_FPS_HUD=2,KYTY_FRAMEGEN_VERBOSE=1,KYTY_RESERVE_CEILING=8000000000,KYTY_GUEST_MOVE=0,KYTY_DRAW_ARGUMENT_SCRATCH=1,KYTY_WRITEBACK_VERIFY=0,KYTY_UPSCALER=dlss,KYTY_DLSS_PRESET=L,KYTY_RENDER_HEIGHT=1440,KYTY_SHADER_WARMUP_BACKGROUND=0,KYTY_PROTECT_SERIAL=1'
$runtimeSettings += ',KYTY_RENDER_HEIGHT=' + $RenderHeight
$runtimeSettings += ',KYTY_FG_HUDLESS=' + [int][bool]$HudlessFrameGen
$runtimeSettings += ',KYTY_SR_TIMING=' + [int][bool]$SrTiming
$runtimeSettings += ',KYTY_FRESH_MEMORY=' + [int](!$DisableFreshMemory)
$launch = @{
    Exe = Join-Path $PSScriptRoot 'kyty_emulator.exe'
    Game = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'game-path.txt') -Raw).Trim()
    Width = 3840; Height = 2160; Fullscreen = $true; AspectFit = $true; Fps120 = $true
    PresentMode = 'Immediate'; RenderHeight = $RenderHeight; Language = 1; Hdr = [bool]$Hdr
    FrameGen = $FrameGenMultiplier - 1; Upscaler = 'dlss'; StreamlineDir = 'C:\Mods\Streamline\_sdk\v2.14.16'
    Set = $runtimeSettings; DryRun = [bool]$DryRun; Follow = [bool]$Follow
}
$colorMode = if ($Hdr) { 'HDR' } else { 'SDR' }
Write-Host "r19: render height $RenderHeight -> 4K, Preset L, $colorMode, fixed ${FrameGenMultiplier}x FG, Reflex Low Latency"
Write-Host "Save folder: $PSScriptRoot\_SaveData (independent copy from r18 when r19 was packaged)"
& (Join-Path $PSScriptRoot 'run-windows.ps1') @launch

