param(
    [Parameter(Mandatory=$true)][string]$ReadOnlySave,
    [Parameter(Mandatory=$true)][string]$TestRoot
)
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'WorldTendency.cs')
Add-Type @'
using System;
using System.Text;
public static class TendencyTestData {
    public static uint Fnv(byte[] data, int start) {
        uint h = 2166136261;
        unchecked { for (int i=start; i<data.Length; i++) h = (h ^ data[i]) * 16777619; }
        return h;
    }
    public static int Offset(byte[] data, int world, bool next) {
        string name = String.Format("TENDENCY_WORLD{0:00}_{1}", world, next ? "LEFTRIGHT" : "BLACKWHITE");
        uint key = Fnv(Encoding.ASCII.GetBytes(name), 0);
        for (int i=16; i<=data.Length-9; i++)
            if (BitConverter.ToUInt32(data,i)==key && data[i+4]==10) return i+5;
        throw new Exception("Missing test field");
    }
    public static void Checksum(byte[] data) {
        Buffer.BlockCopy(BitConverter.GetBytes(Fnv(data,16)),0,data,4,4);
    }
}
'@
if (Test-Path -LiteralPath $TestRoot) { throw 'TestRoot must be new.' }
$original = [IO.File]::ReadAllBytes($ReadOnlySave)
$originalHash = [KytyLocal.WorldTendency]::Hash($original)
$testSave = Join-Path $TestRoot '_SaveData\PPSA01341\SAVEDATA0PlayerProfile0\USR-DATA'
New-Item -ItemType Directory -Path (Split-Path -Parent $testSave) | Out-Null
$script:checks = 0
function Assert($condition, $message) {
    if (!$condition) { throw $message }
    $script:checks++
}
function Reset-Save { [IO.File]::WriteAllBytes($testSave, $original) }
function Changes($worlds, $value) {
    $dict = New-Object 'System.Collections.Generic.Dictionary[int,int]'
    foreach ($world in $worlds) { $dict.Add($world, $value) }
    return ,$dict
}
function Reject($action, $expectedMessage) {
    $before = [KytyLocal.WorldTendency]::Hash([IO.File]::ReadAllBytes($testSave))
    $failed = $false
    try { & $action } catch {
        $failed = $true
        Assert ($_.Exception.GetBaseException().Message -match $expectedMessage) "Wrong rejection: $_"
    }
    Assert $failed 'Expected rejection did not occur.'
    Assert ([KytyLocal.WorldTendency]::Hash([IO.File]::ReadAllBytes($testSave)) -eq $before) 'Rejected operation changed the save.'
}
Reset-Save
$initial = [KytyLocal.WorldTendency]::Read($testSave)
Assert ($initial.Hash -eq $originalHash) 'Read changed save bytes.'
foreach ($value in @(0..10 | ForEach-Object { $_ * 10 })) {
    Reset-Save
    $change = Changes @(1,2,3,4,5) $value
    $result = [KytyLocal.WorldTendency]::Apply($testSave, $change, $originalHash)
    $after = [KytyLocal.WorldTendency]::Read($testSave)
    $actual = [IO.File]::ReadAllBytes($testSave)
    $expected = [byte[]]$original.Clone()
    foreach ($world in 1..5) {
        foreach ($next in @($false,$true)) {
            $offset = [TendencyTestData]::Offset($original, $world, $next)
            [Array]::Copy([BitConverter]::GetBytes([single]($value * 4 - 200)),0,$expected,$offset,4)
        }
        Assert ($after.Current[$world] -eq ($value * 4 - 200) -and $after.Next[$world] -eq ($value * 4 - 200)) 'Wrong current/next tendency.'
    }
    [TendencyTestData]::Checksum($expected)
    Assert ([KytyLocal.WorldTendency]::Hash($actual) -eq [KytyLocal.WorldTendency]::Hash($expected)) 'Unrelated bytes changed.'
    Assert ($actual.Length -eq $original.Length) 'Save length changed.'
    if ($result.Changed) {
        Assert ([KytyLocal.WorldTendency]::Hash([IO.File]::ReadAllBytes((Join-Path $result.BackupDirectory 'USR-DATA'))) -eq $originalHash) 'Backup mismatch.'
        Assert (Test-Path -LiteralPath (Join-Path $result.BackupDirectory 'APPLIED.txt')) 'Missing edit audit.'
        [void][KytyLocal.WorldTendency]::Undo($result)
        Assert ([KytyLocal.WorldTendency]::Read($testSave).Hash -eq $originalHash) 'Undo did not restore exact bytes.'
    }
}
foreach ($world in 1..5) {
    Reset-Save
    [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @($world) 0), $originalHash)
    $after = [KytyLocal.WorldTendency]::Read($testSave)
    foreach ($other in 0..6) {
        if ($other -eq $world) { continue }
        Assert ($after.Current[$other] -eq $initial.Current[$other] -and $after.Next[$other] -eq $initial.Next[$other]) 'Another world changed.'
    }
}
Reset-Save
$edit = [KytyLocal.WorldTendency]::Apply($testSave, (Changes @(1) 100), $originalHash)
$same = [KytyLocal.WorldTendency]::Apply($testSave, (Changes @(1) 100), $edit.AfterHash)
Assert (!$same.Changed -and !$same.BackupDirectory) 'No-op created a backup.'
Reject { [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @(2) 0), $originalHash) } 'changed since'
$newEdit = [KytyLocal.WorldTendency]::Apply($testSave, (Changes @(2) 0), $edit.AfterHash)
Reject { [void][KytyLocal.WorldTendency]::Undo($edit) } 'changed since'
foreach ($world in @(0,6,7)) {
    Reject { [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @($world) 0), $newEdit.AfterHash) } 'Only the five worlds'
}
Reject { [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @(1) 99), $newEdit.AfterHash) } 'Only the five worlds'
foreach ($invalid in @(-10,110)) {
    Reject { [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @(1) $invalid), $newEdit.AfterHash) } 'Only the five worlds'
}
foreach ($kind in @('checksum','version','size','duplicate','nan','range','type','layout','truncated')) {
    $bad = [byte[]]$original.Clone()
    $offset = [TendencyTestData]::Offset($bad, 1, $false)
    switch ($kind) {
        checksum { $bad[100] = $bad[100] -bxor 1 }
        version { $bad[0] = 8 }
        size { $bad[8] = $bad[8] -bxor 1 }
        duplicate { [Array]::Copy($bad,$offset-5,$bad,16,9) }
        nan { [Array]::Copy([BitConverter]::GetBytes([single]::NaN),0,$bad,$offset,4) }
        range { [Array]::Copy([BitConverter]::GetBytes([single]201),0,$bad,$offset,4) }
        type { $bad[$offset-1] = 4 }
        layout {
            $a = [byte[]]::new(9)
            [Array]::Copy($bad,$offset-5,$a,0,9)
            [Array]::Copy($bad,$offset+4,$bad,$offset-5,9)
            [Array]::Copy($a,0,$bad,$offset+4,9)
        }
        truncated { $bad = [byte[]]$bad[0..100] }
    }
    if ($kind -ne 'checksum') { [TendencyTestData]::Checksum($bad) }
    [IO.File]::WriteAllBytes($testSave,$bad)
    Reject { [void][KytyLocal.WorldTendency]::Apply($testSave, (Changes @(1) 0), $originalHash) } 'checksum|header|Duplicate|range|Missing|layout|size'
}
Reset-Save
$lock = [IO.File]::Open($testSave,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {
    $failed = $false
    try { [void][KytyLocal.WorldTendency]::Apply($testSave,(Changes @(1) 100),$originalHash) } catch { $failed = $true }
    Assert $failed 'Locked save was editable.'
} finally { $lock.Dispose() }
Assert ([KytyLocal.WorldTendency]::Read($testSave).Hash -eq $originalHash) 'Locked save changed.'
$stubPath = Join-Path $TestRoot 'kyty_emulator.exe'
Add-Type -OutputAssembly $stubPath -OutputType WindowsApplication -TypeDefinition @'
public static class TendencyProcessGuardStub {
    public static void Main() { System.Threading.Thread.Sleep(30000); }
}
'@
$stub = Start-Process -FilePath $stubPath -WindowStyle Hidden -PassThru
try {
    Assert (!$stub.HasExited) 'Process guard helper did not start.'
    Reject { [void][KytyLocal.WorldTendency]::Apply($testSave,(Changes @(1) 100),$originalHash) } 'Close the game'
} finally {
    if (!$stub.HasExited) { $stub.Kill(); $stub.WaitForExit() }
    $stub.Dispose()
}
Assert ([KytyLocal.WorldTendency]::Hash([IO.File]::ReadAllBytes($ReadOnlySave)) -eq $originalHash) 'Original user save changed.'
Write-Output "PASS: $script:checks assertions, all on disposable copies; original save hash $originalHash."
