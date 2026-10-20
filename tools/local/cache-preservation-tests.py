"""Run the production PowerShell hint/prune decisions with a fake CLI only.

No emulator, shader precompiler, Vulkan, or real cache is opened. Both installed
PowerShell versions run the exact decision blocks extracted from the source.
"""
import base64
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2]
SCRIPT = SOURCE / 'precompile-windows.ps1'
HOSTS = tuple(dict.fromkeys(filter(None, (shutil.which('pwsh'), shutil.which('powershell')))))


class CachePreservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not HOSTS:
            raise unittest.SkipTest('PowerShell is required')
        text = SCRIPT.read_text(encoding='utf-8-sig')
        cls.hints = text.split('$recordedLost = $false\n', 1)[1].split('# The pipeline recipes', 1)[0]
        cls.merge = text.split("$mergeArguments = @('--merge', '--stamp')", 1)[1].split('Wait-Precompile', 1)[0]

    def decision(self, host, *, release=True, local=True, manual=False,
                 release_result=0, make_result=0, apply_result=0, quick=False):
        fixture_parent = SOURCE / '_Build' / 'cache-preservation-tests'
        fixture_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="kyty cache's preservation ", dir=fixture_parent) as tmp:
            root = Path(tmp)
            self.assertEqual(root.resolve().parent, fixture_parent)
            game_id = 'PPSA01342_01.005.000'
            if release:
                path = root / 'tools/local/static-precompile' / f'hints-{game_id}.hints'
                path.parent.mkdir(parents=True)
                path.touch()
            if local:
                path = root / '_PipelineCache/warmup-v2/test-device' / f'{game_id}.shaders'
                path.parent.mkdir(parents=True)
                path.touch()
            if manual:
                (root / f'recorded-{game_id}.seeds').touch()
            payload = dict(root=str(root), hints=self.hints, merge=self.merge,
                           script=str(SCRIPT), release_result=release_result,
                           make_result=make_result, apply_result=apply_result,
                           quick=quick)
            encoded = base64.b64encode(json.dumps(payload).encode()).decode()
            command = r'''
$ErrorActionPreference = 'Stop'
$case = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('PAYLOAD')) | ConvertFrom-Json
$tokens = $null; $parseErrors = $null
$null = [System.Management.Automation.Language.Parser]::ParseFile($case.script, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
$gameId = 'PPSA01342_01.005.000'
$info = [pscustomobject]@{titleId='PPSA01342'; contentVersion='01.005.000'}
$Game = Join-Path $PSScriptRoot 'game folder'
$Seeds = Join-Path $PSScriptRoot "seeds-$gameId.seeds"
$Recorded = '*'
$Quick = $case.quick
$Exe = 'Invoke-HintFixture'
$script:calls = @()
function Invoke-HintFixture {
    $cliArgs = @($args)
    $script:calls += ,$cliArgs
    if ($cliArgs -contains '--make-hints') {
        $global:LASTEXITCODE = [int]$case.make_result
    } elseif ($cliArgs -contains '--apply-hints') {
        $inputPath = $cliArgs[[array]::IndexOf($cliArgs, '--apply-hints') + 1]
        if ((Split-Path $inputPath -Leaf) -like 'recorded-play-*') {
            $global:LASTEXITCODE = [int]$case.apply_result
        } else {
            $global:LASTEXITCODE = [int]$case.release_result
        }
    } else { throw 'unexpected fake CLI command' }
}
$recordedLost = $false
HINT_BLOCK
$mergeArguments = @('--merge', '--stamp')
MERGE_BLOCK
[pscustomobject]@{
    lost=$recordedLost
    usable=[bool]$Recorded
    prune=($mergeArguments -contains '--prune')
    stamp=($mergeArguments -contains '--stamp')
    calls=$script:calls.Count
} | ConvertTo-Json -Compress
'''.replace('PAYLOAD', encoded).replace('HINT_BLOCK', self.hints).replace('MERGE_BLOCK', self.merge)
            fixture = root / 'hint fixture.ps1'
            fixture.write_text(command, encoding='utf-8-sig')
            completed = subprocess.run([host, '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                                        '-File', str(fixture)],
                                       text=True, capture_output=True, timeout=20)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            return json.loads(completed.stdout.strip().splitlines()[-1])

    def assert_decision(self, expected, **kwargs):
        return_value = None
        for host in HOSTS:
            with self.subTest(host=host, **kwargs):
                result = self.decision(host, **kwargs)
                self.assertEqual((result['lost'], result['usable'], result['prune']), expected)
                self.assertTrue(result['stamp'])
                return_value = result
        return return_value

    def test_complete_union_can_prune(self):
        self.assert_decision((False, True, True))

    def test_partial_union_keeps_valid_inputs_without_pruning(self):
        self.assert_decision((True, True, False), make_result=2)

    def test_lost_release_keeps_local_inputs_without_pruning(self):
        self.assert_decision((True, True, False), release_result=1)

    def test_lost_release_with_no_local_inputs_keeps_old_store(self):
        self.assert_decision((True, False, False), release_result=1, local=False)

    def test_failed_union_keeps_old_store(self):
        self.assert_decision((True, False, False), make_result=1)

    def test_failed_apply_after_partial_union_keeps_old_store(self):
        self.assert_decision((True, False, False), make_result=2, apply_result=1)

    def test_failed_apply_after_complete_union_keeps_old_store(self):
        self.assert_decision((True, False, False), apply_result=1)

    def test_release_only_is_used_without_union(self):
        result = self.assert_decision((False, True, True), local=False)
        self.assertEqual(result['calls'], 1)

    def test_local_only_is_used(self):
        self.assert_decision((False, True, True), release=False)

    def test_manual_only_is_used(self):
        self.assert_decision((False, True, True), release=False, local=False, manual=True)

    def test_no_recordings_is_valid_seed_only_run(self):
        result = self.assert_decision((False, False, True), release=False, local=False)
        self.assertEqual(result['calls'], 0)

    def test_quick_never_prunes(self):
        self.assert_decision((False, True, False), quick=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)
