// Demon's Souls remake PPSA01341, save format 7. See World-Tendency.md.
// Operates on a closed game's save, never on process memory or the game binary.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace KytyLocal {
    public sealed class TendencySnapshot {
        public string Path;
        public string Hash;
        public float[] Current;
        public float[] Next;
        internal int Start;
        internal byte[] Data;
    }

    public sealed class TendencyEdit {
        public string SavePath;
        public string BeforeHash;
        public string AfterHash;
        public string BackupDirectory;
        public bool Changed;
    }

    public static class WorldTendency {
        // The game's world numbering differs from its archstone/menu numbering.
        public static readonly int[] Worlds = { 1, 5, 3, 2, 4 };
        public static readonly string[] Names = {
            "Boletarian Palace", "Stonefang Tunnel", "Tower of Latria",
            "Shrine of Storms", "Valley of Defilement"
        };

        static uint Fnv(byte[] bytes, int start, int count) {
            uint h = 2166136261;
            unchecked {
                for (int i = start; i < start + count; ++i) h = (h ^ bytes[i]) * 16777619;
            }
            return h;
        }

        static byte[] FieldKey(int world, bool next) {
            byte[] name = Encoding.ASCII.GetBytes(String.Format(
                "TENDENCY_WORLD{0:00}_{1}", world, next ? "LEFTRIGHT" : "BLACKWHITE"));
            return BitConverter.GetBytes(Fnv(name, 0, name.Length));
        }

        public static string Hash(byte[] bytes) {
            using (SHA256 sha = SHA256.Create()) {
                return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
            }
        }

        static string CheckedPath(string path) {
            path = System.IO.Path.GetFullPath(path);
            DirectoryInfo slot = Directory.GetParent(path);
            if (System.IO.Path.GetFileName(path) != "USR-DATA" ||
                slot == null || !Regex.IsMatch(slot.Name, @"^SAVEDATA0PlayerProfile(?:[0-9]|1[01])$") ||
                slot.Parent == null || slot.Parent.Name != "PPSA01341" ||
                slot.Parent.Parent == null || slot.Parent.Parent.Name != "_SaveData")
                throw new InvalidDataException("Select a PPSA01341 character slot inside this build's _SaveData folder.");
            return path;
        }

        static byte[] ReadBytes(Stream stream) {
            if (stream.Length < 142 || stream.Length > 4 * 1024 * 1024)
                throw new InvalidDataException("Unsupported save size.");
            byte[] data = new byte[(int)stream.Length];
            int at = 0;
            while (at < data.Length) {
                int read = stream.Read(data, at, data.Length - at);
                if (read == 0) throw new EndOfStreamException();
                at += read;
            }
            return data;
        }

        static TendencySnapshot Parse(string path, byte[] data) {
            if (data.Length < 142 || BitConverter.ToUInt32(data, 0) != 7 ||
                BitConverter.ToUInt32(data, 8) != data.Length - 16 ||
                BitConverter.ToUInt32(data, 12) != 1)
                throw new InvalidDataException("Unsupported save header. No changes made.");
            if (BitConverter.ToUInt32(data, 4) != Fnv(data, 16, data.Length - 16))
                throw new InvalidDataException("Save checksum does not match. No changes made.");

            int start = -1;
            float[] current = new float[7], next = new float[7];
            for (int world = 0; world < 7; ++world) {
                for (int axis = 0; axis < 2; ++axis) {
                    byte[] key = FieldKey(world, axis == 1);
                    int found = -1;
                    for (int p = 16; p <= data.Length - 9; ++p) {
                        if (data[p] != key[0] || data[p + 1] != key[1] ||
                            data[p + 2] != key[2] || data[p + 3] != key[3] || data[p + 4] != 10)
                            continue;
                        if (found != -1) throw new InvalidDataException("Duplicate tendency field. No changes made.");
                        found = p;
                    }
                    if (found == -1) throw new InvalidDataException("Missing tendency field. No changes made.");
                    if (world == 0 && axis == 0) start = found;
                    if (found != start + (world * 2 + axis) * 9)
                        throw new InvalidDataException("Unrecognized tendency layout. No changes made.");
                    float value = BitConverter.ToSingle(data, found + 5);
                    if (Single.IsNaN(value) || Single.IsInfinity(value) || value < -200 || value > 200)
                        throw new InvalidDataException("Tendency is outside the verified range. No changes made.");
                    (axis == 0 ? current : next)[world] = value;
                }
            }
            return new TendencySnapshot {
                Path = path, Hash = Hash(data), Current = current, Next = next,
                Start = start, Data = data
            };
        }

        public static TendencySnapshot Read(string path) {
            path = CheckedPath(path);
            using (FileStream file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                return Parse(path, ReadBytes(file));
        }

        static void RequireClosedGame() {
            foreach (string name in new [] { "kyty_emulator", "kyty_shader_precompile" }) {
                Process[] processes = Process.GetProcessesByName(name);
                bool running = processes.Length != 0;
                foreach (Process p in processes) p.Dispose();
                if (running) throw new InvalidOperationException("Close the game and precompiler before editing tendency.");
            }
        }

        static void WriteDurable(string path, byte[] data) {
            using (FileStream file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None)) {
                file.Write(data, 0, data.Length);
                file.Flush(true);
            }
        }

        static void CheckOnlyWorldChanges(TendencySnapshot before, TendencySnapshot after) {
            if (before.Data.Length != after.Data.Length || before.Start != after.Start)
                throw new InvalidDataException("Save layout changed.");
            for (int p = 0; p < before.Data.Length; ++p) {
                if (before.Data[p] == after.Data[p] || (p >= 4 && p < 8)) continue;
                int relative = p - before.Start;
                int entry = relative / 9;
                int fieldByte = relative % 9;
                if (relative < 0 || entry < 2 || entry > 11 || fieldByte < 5)
                    throw new InvalidDataException("An unrelated save field would change. Refusing edit.");
            }
        }

        static TendencyEdit Commit(TendencySnapshot before, byte[] updated, string expectedHash, string reason) {
            TendencySnapshot after = Parse(before.Path, updated);
            CheckOnlyWorldChanges(before, after);
            if (!String.Equals(before.Hash, expectedHash, StringComparison.Ordinal))
                throw new InvalidOperationException("The save changed since it was displayed. Reload it before applying.");
            TendencyEdit result = new TendencyEdit {
                SavePath = before.Path, BeforeHash = before.Hash, AfterHash = after.Hash,
                Changed = before.Hash != after.Hash
            };
            if (!result.Changed) return result;
            RequireClosedGame();
            string package = Directory.GetParent(before.Path).Parent.Parent.Parent.FullName;
            string backup = System.IO.Path.Combine(package, "backups", "world-tendency",
                DateTime.Now.ToString("yyyyMMdd-HHmmss-fff") + "-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            Directory.CreateDirectory(backup);
            string backupFile = System.IO.Path.Combine(backup, "USR-DATA");
            WriteDurable(backupFile, before.Data);
            if (Hash(File.ReadAllBytes(backupFile)) != before.Hash)
                throw new IOException("Backup verification failed; save was not changed.");
            result.BackupDirectory = backup;
            StringBuilder audit = new StringBuilder();
            audit.AppendLine(reason);
            audit.AppendLine("Time: " + DateTimeOffset.Now.ToString("o"));
            audit.AppendLine("Save: " + before.Path);
            audit.AppendLine("Before SHA256: " + before.Hash);
            audit.AppendLine("After SHA256: " + after.Hash);
            for (int i = 0; i < Worlds.Length; ++i) {
                int w = Worlds[i];
                audit.AppendLine(String.Format(System.Globalization.CultureInfo.InvariantCulture,
                    "{0}: current {1} -> {2}; next {3} -> {4}",
                    Names[i], before.Current[w], after.Current[w], before.Next[w], after.Next[w]));
            }
            WriteDurable(System.IO.Path.Combine(backup, "edit.txt"), Encoding.UTF8.GetBytes(audit.ToString()));
            string temp = before.Path + ".tendency-" + Guid.NewGuid().ToString("N") + ".tmp";
            try {
                WriteDurable(temp, updated);
                if (Hash(File.ReadAllBytes(temp)) != after.Hash)
                    throw new IOException("Staged save verification failed.");
                // Allow atomic replacement, while denying other readers and writers until it finishes.
                using (FileStream original = new FileStream(before.Path, FileMode.Open, FileAccess.Read, FileShare.Delete)) {
                    if (Hash(ReadBytes(original)) != before.Hash)
                        throw new InvalidOperationException("The save changed during the edit. No changes applied.");
                    RequireClosedGame();
                    File.Replace(temp, before.Path, null);
                }
                if (Read(before.Path).Hash != after.Hash)
                    throw new IOException("Post-write verification failed. Original save is preserved at " + backup);
                File.WriteAllText(System.IO.Path.Combine(backup, "APPLIED.txt"), DateTimeOffset.Now.ToString("o"));
                return result;
            } finally {
                if (File.Exists(temp)) File.Delete(temp);
            }
        }

        public static TendencyEdit Apply(string path, IDictionary<int, int> changes, string expectedHash) {
            RequireClosedGame();
            if (changes == null || changes.Count == 0) throw new ArgumentException("Select at least one world.");
            TendencySnapshot before = Read(path);
            byte[] updated = (byte[])before.Data.Clone();
            foreach (KeyValuePair<int, int> change in changes) {
                if (Array.IndexOf(Worlds, change.Key) < 0 ||
                    change.Value < 0 || change.Value > 100 || change.Value % 10 != 0)
                    throw new ArgumentException("Only the five worlds and values from 0 to 100 in steps of 10 are supported.");
                for (int axis = 0; axis < 2; ++axis)
                    Buffer.BlockCopy(BitConverter.GetBytes((float)(change.Value * 4 - 200)), 0, updated,
                        before.Start + (change.Key * 2 + axis) * 9 + 5, 4);
            }
            Buffer.BlockCopy(BitConverter.GetBytes(Fnv(updated, 16, updated.Length - 16)), 0, updated, 4, 4);
            return Commit(before, updated, expectedHash, "World Tendency edit");
        }

        public static TendencyEdit Undo(TendencyEdit edit) {
            RequireClosedGame();
            if (edit == null || !edit.Changed || String.IsNullOrEmpty(edit.BackupDirectory))
                throw new ArgumentException("No edit to undo.");
            TendencySnapshot before = Read(edit.SavePath);
            byte[] original = File.ReadAllBytes(System.IO.Path.Combine(edit.BackupDirectory, "USR-DATA"));
            if (Hash(original) != edit.BeforeHash) throw new InvalidDataException("Backup checksum does not match.");
            return Commit(before, original, edit.AfterHash, "Undo World Tendency edit");
        }
    }
}
