# World Tendency panel

Open **World Tendency** in the local launcher, select the character slot, choose
values from **0 (Pure Black)** through **50 (Neutral)** to **100 (Pure White)**
in steps of 10 for any of the five worlds, and click
**Apply** with the emulator closed. **Keep current** leaves that world alone.
Load the character afterwards. This is a one-time save edit, not a permanent lock:
subsequent gameplay can change tendency normally.
After applying, click **Back to launcher** (or press Escape), then **Play**.
Going back does not apply any selections you have not yet applied.

The panel edits only the selected world's current and next-visit values. It
does not reset completed events, respawn defeated bosses/NPCs, change Character
Tendency, or alter other progression flags. Switching tendency can naturally affect
which events/enemies the game makes available.

Before each change, it writes and verifies an exact original `USR-DATA` backup
under `backups/world-tendency/<timestamp>/`. `edit.txt` records the slot, original
and resulting SHA256, and all five before/after tendency pairs. `APPLIED.txt`
marks a completed edit. The replacement is staged and atomically installed.
Run `.settings.json` files also record each character slot's saved world
tendencies and save hash at launch; this is a startup snapshot, not live tracking.
**Undo this edit** works within the open panel only while the save still matches
the edit's output, preventing it from overwriting later gameplay progress.
Backups persist after closing the panel. To restore one manually with the game
closed, use the `Save:` path in that backup's `edit.txt`; restoring the entire
file also restores progress to that backup's time.

Supported format: PPSA01341 player slots 0–11, version-7 `USR-DATA`, verified
against game 01.005.000. Unknown headers, bad checksums, missing/duplicate or
noncontiguous fields, and out-of-range values fail closed. `.blocks` is capacity
metadata, not the character payload, and is not edited.

## Local reverse-engineering evidence

Audited eboot SHA256:
`DAA99EEC682002B5A0DCFF4DE5C49A1CD55E56E3371760D8840E224D4001F199`.
No executable bytes or copyrighted game data are included in this tool.

- SELF code segment: file `0x10280`, RVA 0. Read-only segment:
  file `0x1f18040`, RVA `0x1f08000`.
- Save header: u32 version 7, FNV-1a32 checksum of bytes 16 through EOF,
  u32 payload byte count, u32 1.
- Loader RVAs `0x2cea3`–`0x2d069` and writer `0x34aa8`–`0x34cbe`
  format `Tendency_World%02d_BlackWhite` / `Tendency_World%02d_LeftRight`,
  uppercase/FNV-1a hash the names, and read/write type `0x0a` float32.
  Seven pairs, each field 9 bytes, in index order 0–6.
- World objects store current values at `+0x18 + 8*i`; constructor
  `0x450236` onwards binds `cp11_tendency_world_current_0` etc.
  The debug bounds are -200 through +200 (e.g. `0x450284`/`0x45028b`).
- Despite its legacy name, the world's serialized LeftRight component carries
  next-visit tendency: writer helper `0x453880` copies next values from
  `+0x50,+0x58,...` into `+0x1c,+0x24,...`; loader helper `0x453910`
  restores them, then clears those temporary components.
- The debug menu at `0x4518a9`, `0x45192c`, `0x4519a2`, `0x451a18`,
  `0x451a8e`, `0x451b04`, `0x451b77` labels indices 0–6 as
  m01/Nexus, m02/Boletaria, m03/Storms, m04/Latria, m05/Valley,
  m06/Stonefang, m07/reserved. Only indices 1–5 are editable.
- Character convars at `0x450025` and `0x450145` bind separate object
  fields `+0x10/+0x14`, outside this world array. They are never edited.

The panel and Apply API use a normalized 0 to 100 scale in steps of 10.
Saved float values are `displayed * 4 - 200`: -200 at displayed 0 (black),
0 at displayed 50 (neutral), and +200 at displayed 100 (white).
Readouts use the inverse `(saved + 200) / 4` without
rounding existing saves to a selectable step. Audit files retain the raw values;
run settings include both raw and normalized values.
Static inspection and copy-based tests validate the save transformations;
visual confirmation of the in-game icons and events remains a user playtest.

## Tests

Run in Windows PowerShell 5.1:

```powershell
.\Test-WorldTendency.ps1 -ReadOnlySave '...\_SaveData\PPSA01341\SAVEDATA0PlayerProfile0\USR-DATA' -TestRoot '...\new-test-folder'
```

Tests use disposable copies, checking exact changed bytes, all presets,
per-world isolation, hash-verified backups, exact undo, no-op, stale save/undo
rejection, invalid values/layouts/header/checksum, locked files, and refusal
while an isolated harmless process named `kyty_emulator` exists.
The helper is stopped by its own process handle. The real game is not launched.
