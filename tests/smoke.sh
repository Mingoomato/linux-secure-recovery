#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin_dir=${BIN_DIR:-"$root_dir/build"}
bin="$bin_dir/sre"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/source/nested" "$tmp/target-dry" "$tmp/target-conflict/nested" "$tmp/target-clean"
printf 'recoverable bytes\n' >"$tmp/source/nested/data.txt"
printf 'backslash path bytes\n' >"$tmp/source/nested/back\\slash.txt"
printf 'unicode path bytes\n' >"$tmp/source/nested/한글.txt"
python3 - "$tmp/source/nested/data.txt" <<'PY'
import os, sys
os.chmod(sys.argv[1], 0o640)
os.utime(sys.argv[1], ns=(1_700_000_000_123456789, 1_700_000_000_123456789))
PY
printf 'do not follow\n' >"$tmp/external.txt"
ln -s "$tmp/external.txt" "$tmp/source/external-link"

ln -s "$tmp/source" "$tmp/source-link"
set +e
"$bin" init --source "$tmp/source-link" --store "$tmp/unsafe-store" >"$tmp/symlink-root.out" 2>&1
rc=$?
set -e
[ "$rc" -ne 0 ]
[ ! -e "$tmp/unsafe-store" ]

"$bin" init --source "$tmp/source" --store "$tmp/store"
snapshot_output=$("$bin" snapshot --source "$tmp/source" --store "$tmp/store" --label smoke)
snapshot_id=${snapshot_output%% *}
[ -n "$snapshot_id" ]
"$bin" show --store "$tmp/store" --snapshot "$snapshot_id" >"$tmp/show.json"
python3 - "$tmp/show.json" "$tmp/source" "$snapshot_id" <<'PY'
import json, pathlib, sys
result = json.load(open(sys.argv[1], encoding="utf-8"))
source = pathlib.Path(sys.argv[2])
expected_bytes = sum(path.stat().st_size for path in source.rglob("*")
                     if path.is_file() and not path.is_symlink())
assert result["snapshot_id"] == sys.argv[3]
assert result["state"] == "COMMITTED" and result["incomplete_count"] == 0
assert result["entry_count"] == 3 and result["byte_count"] == expected_bytes
PY
"$bin" verify --store "$tmp/store" --snapshot "$snapshot_id" >"$tmp/verify.json"
python3 - "$tmp/verify.json" <<'PY'
import json, sys
result = json.load(open(sys.argv[1], encoding="utf-8"))
assert result["bad"] == 0 and result["verified"] == 3
PY
python3 - "$tmp/store/manifests/$snapshot_id.json" "$root_dir/contracts/manifest-v1.schema.json" <<'PY'
import hashlib, json, pathlib, sys
from jsonschema import Draft202012Validator
manifest = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
schema = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
Draft202012Validator.check_schema(schema)
Draft202012Validator(schema).validate(manifest)
assert manifest["entry_count"] == len(manifest["entries"]) == 3
assert manifest["incomplete_count"] == 0
integrity = manifest.pop("manifest_integrity_hash")
canonical = json.dumps(manifest, ensure_ascii=False, separators=(",", ":"))
assert hashlib.sha256(canonical.encode()).hexdigest() == integrity
assert set(schema["required"]).issubset(set(json.loads(pathlib.Path(sys.argv[1]).read_text()).keys()))
PY

# Sidecar tampering must make verify fail and must stop restore before target changes.
cp "$tmp/store/manifests/$snapshot_id.json" "$tmp/manifest-backup.json"
printf '{"schema_version":"manifest.v1","tampered":true}\n' >"$tmp/store/manifests/$snapshot_id.json"
set +e
"$bin" verify --store "$tmp/store" --snapshot "$snapshot_id" >"$tmp/manifest-corrupt-verify.json"
rc=$?
set -e
[ "$rc" -eq 2 ]
grep -q 'corrupt manifest' "$tmp/manifest-corrupt-verify.json"
mkdir -p "$tmp/target-manifest/nested"
printf 'preserve manifest target\n' >"$tmp/target-manifest/nested/data.txt"
set +e
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-manifest" --confirm --overwrite >"$tmp/manifest-corrupt-restore.out" 2>&1
rc=$?
set -e
[ "$rc" -eq 2 ]
grep -q 'preserve manifest target' "$tmp/target-manifest/nested/data.txt"
cp "$tmp/manifest-backup.json" "$tmp/store/manifests/$snapshot_id.json"

"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-dry" --dry-run >"$tmp/dry.json"
[ ! -e "$tmp/target-dry/nested/data.txt" ]
printf 'existing bytes\n' >"$tmp/target-conflict/nested/data.txt"
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-conflict" --confirm >"$tmp/conflict.json"
grep -q 'existing bytes' "$tmp/target-conflict/nested/data.txt"
python3 - "$tmp/conflict.json" <<'PY'
import json, sys
result = json.loads(open(sys.argv[1], encoding="utf-8").read().splitlines()[-1])
assert result["conflicts"] == 1 and result["restored"] == 2
PY
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-clean" --confirm >"$tmp/restore.json"
cmp "$tmp/source/nested/data.txt" "$tmp/target-clean/nested/data.txt"
cmp "$tmp/source/nested/back\\slash.txt" "$tmp/target-clean/nested/back\\slash.txt"
cmp "$tmp/source/nested/한글.txt" "$tmp/target-clean/nested/한글.txt"
[ ! -e "$tmp/target-clean/external-link" ]
python3 - "$tmp/source/nested/data.txt" "$tmp/target-clean/nested/data.txt" <<'PY'
import os, stat, sys
source, restored = (os.stat(path) for path in sys.argv[1:])
assert stat.S_IMODE(source.st_mode) == stat.S_IMODE(restored.st_mode) == 0o640
assert source.st_mtime_ns == restored.st_mtime_ns
PY

# Keep a second snapshot so prune can prove it preserves shared content blobs.
sleep 1
extra_snapshot_output=$($bin snapshot --source "$tmp/source" --store "$tmp/store" --label keep)
extra_snapshot_id=${extra_snapshot_output%% *}
[ -n "$extra_snapshot_id" ]

# Corrupt a committed content-addressed blob; verify and staged restore must reject it.
python3 - "$tmp/store/metadata.sqlite" "$tmp/store" "$tmp/source/nested/data.txt" <<'PY'
import pathlib, sqlite3, sys
db = sqlite3.connect(sys.argv[1])
hash_value = db.execute(
    "SELECT sha256 FROM entries WHERE relative_path='nested/data.txt' ORDER BY snapshot_id DESC LIMIT 1"
).fetchone()[0]
blob = pathlib.Path(sys.argv[2]) / 'blobs' / 'sha256' / hash_value[:2] / hash_value
assert blob.read_bytes() == pathlib.Path(sys.argv[3]).read_bytes()
blob.write_bytes(b'intentionally corrupted bytes\n')
PY
set +e
"$bin" verify --store "$tmp/store" --snapshot "$snapshot_id" >"$tmp/corrupt-verify.json"
rc=$?
set -e
[ "$rc" -eq 2 ]
python3 - "$tmp/corrupt-verify.json" <<'PY'
import json, sys
lines = open(sys.argv[1], encoding="utf-8").read().splitlines()
result = json.loads(lines[-1])
assert result["bad"] == 1
assert any(line.startswith("corrupt nested/data.txt") for line in lines[:-1])
PY
mkdir -p "$tmp/target-corrupt/nested"
printf 'preserve original\n' >"$tmp/target-corrupt/nested/data.txt"
set +e
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-corrupt" --confirm --overwrite >"$tmp/corrupt-restore.json"
rc=$?
set -e
[ "$rc" -eq 2 ]
grep -q 'preserve original' "$tmp/target-corrupt/nested/data.txt"
python3 - "$tmp/corrupt-restore.json" <<'PY'
import json, sys
result = json.loads(open(sys.argv[1], encoding="utf-8").read().splitlines()[-1])
assert result["failed"] == 1
PY

# Restore the test fixture blob, then prune the older snapshot and validate audit evidence.
python3 - "$tmp/store/metadata.sqlite" "$tmp/store" "$tmp/source/nested/data.txt" "$snapshot_id" "$extra_snapshot_id" <<'PY'
import pathlib, sqlite3, sys
db = sqlite3.connect(sys.argv[1])
snapshot_id, extra_id = sys.argv[4:6]
hash_value = db.execute("SELECT sha256 FROM entries WHERE snapshot_id=? AND relative_path='nested/data.txt'",
                        (snapshot_id,)).fetchone()[0]
blob = pathlib.Path(sys.argv[2]) / 'blobs' / 'sha256' / hash_value[:2] / hash_value
blob.write_bytes(pathlib.Path(sys.argv[3]).read_bytes())
db.commit()
db.close()
PY
"$bin" prune --store "$tmp/store" --keep 1
[ ! -e "$tmp/store/manifests/$snapshot_id.json" ]
[ -e "$tmp/store/manifests/$extra_snapshot_id.json" ]
"$bin" verify --store "$tmp/store" --snapshot "$extra_snapshot_id" >"$tmp/kept-verify.json"
python3 - "$tmp/store/audit/events.ndjson" "$root_dir/contracts/operation-v1.schema.json" "$tmp/source" "$snapshot_id" <<'PY'
import json, pathlib, stat, sys
from jsonschema import Draft202012Validator
audit_path = pathlib.Path(sys.argv[1])
schema_path = pathlib.Path(sys.argv[2])
source_dir = pathlib.Path(sys.argv[3])
pruned_id = sys.argv[4]
schema = json.loads(schema_path.read_text(encoding="utf-8"))
Draft202012Validator.check_schema(schema)
validator = Draft202012Validator(schema)
records = [json.loads(line) for line in audit_path.read_text(encoding="utf-8").splitlines()]
assert records
for record in records:
    validator.validate(record)
source_bytes = sum(path.stat().st_size for path in source_dir.rglob("*")
                   if path.is_file() and not path.is_symlink())
snapshot = next(item for item in records if item["operation"] == "snapshot" and item["snapshot_id"] == pruned_id)
assert snapshot["affected_count"] == 3 and snapshot["byte_count"] == source_bytes
restores = [item for item in records if item["operation"] == "restore"]
assert any(item["affected_count"] == 3 and item["byte_count"] == source_bytes and item["confirmed"] for item in restores)
prune = next(item for item in records if item["operation"] == "prune")
assert prune["affected_count"] == 1 and prune["byte_count"] == source_bytes and prune["confirmed"]
mode = stat.S_IMODE(audit_path.stat().st_mode)
assert mode == 0o600, oct(mode)
PY

# Simulate process death after the durable COPYING row, before its entry transaction.
printf 'stale temporary bytes\n' >"$tmp/store/temp/stale-crash.part"
printf 'stale manifest temporary bytes\n' >"$tmp/store/temp/stale-verified.manifest.part"
printf '{"orphan":true}\n' >"$tmp/store/manifests/stale-verified.json"
printf '{"orphan":true}\n' >"$tmp/store/manifests/pruned-orphan.json"
ln -s "$tmp/external.txt" "$tmp/store/temp/stale-link.part"
python3 - "$tmp/store/metadata.sqlite" <<'PY'
import sqlite3, sys, time
db = sqlite3.connect(sys.argv[1])
db.execute("INSERT INTO snapshots(id,source_root,created_at,state,incomplete_count) VALUES(?,?,?,?,?)",
           ("stale-crash", "/synthetic/source", int(time.time()), "COPYING", 0))
db.execute("INSERT INTO snapshots(id,source_root,created_at,state,incomplete_count) VALUES(?,?,?,?,?)",
           ("stale-verified", "/synthetic/source", int(time.time()), "VERIFIED", 0))
db.commit()
db.close()
PY
"$bin" doctor --store "$tmp/store" >"$tmp/doctor.json"
python3 - "$tmp/doctor.json" "$tmp/store/metadata.sqlite" <<'PY'
import json, sqlite3, sys
result = json.load(open(sys.argv[1], encoding="utf-8"))
assert result["recovered_incomplete"] == 2
assert result["removed_temp_files"] == 3
assert result["removed_orphan_manifests"] == 2
db = sqlite3.connect(sys.argv[2])
state, incomplete = db.execute("SELECT state,incomplete_count FROM snapshots WHERE id='stale-crash'").fetchone()
assert (state, incomplete) == ("FAILED", 1)
state, incomplete = db.execute("SELECT state,incomplete_count FROM snapshots WHERE id='stale-verified'").fetchone()
assert (state, incomplete) == ("FAILED", 1)
PY
[ ! -e "$tmp/store/temp/stale-crash.part" ]
[ ! -e "$tmp/store/temp/stale-verified.manifest.part" ]
[ ! -e "$tmp/store/temp/stale-link.part" ]
[ ! -e "$tmp/store/manifests/stale-verified.json" ]
[ ! -e "$tmp/store/manifests/pruned-orphan.json" ]
[ -e "$tmp/external.txt" ]
set +e
"$bin" verify --store "$tmp/store" --snapshot stale-crash >"$tmp/stale-verify.out" 2>&1
rc=$?
set -e
[ "$rc" -ne 0 ]

# A second process must wait for the exclusive per-store operation lock.
python3 - "$bin" "$tmp/store" <<'PY'
import fcntl, subprocess, sys, time
lock = open(sys.argv[2] + "/.operation.lock", "r+")
fcntl.flock(lock, fcntl.LOCK_EX)
proc = subprocess.Popen([sys.argv[1], "doctor", "--store", sys.argv[2]],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
time.sleep(0.15)
assert proc.poll() is None, "doctor did not wait for the store lock"
fcntl.flock(lock, fcntl.LOCK_UN)
stdout, stderr = proc.communicate(timeout=5)
assert proc.returncode == 0, stderr
assert '"store":"ready"' in stdout
PY

# A filename with invalid UTF-8 bytes must not produce an invalid JSON sidecar.
mkdir -p "$tmp/invalid-source" "$tmp/invalid-store"
python3 - "$tmp/invalid-source" <<'PY'
import os, sys
fd = os.open(os.fsencode(sys.argv[1]) + b"/bad-\xff-name", os.O_CREAT | os.O_WRONLY, 0o600)
os.write(fd, b"opaque filename bytes\n")
os.close(fd)
PY
"$bin" init --source "$tmp/invalid-source" --store "$tmp/invalid-store"
set +e
"$bin" snapshot --source "$tmp/invalid-source" --store "$tmp/invalid-store" >"$tmp/invalid-name.out" 2>&1
rc=$?
set -e
[ "$rc" -ne 0 ]
grep -q 'not valid UTF-8' "$tmp/invalid-name.out"
"$bin" prune --store "$tmp/invalid-store" --keep 0
python3 - "$tmp/invalid-store/metadata.sqlite" "$tmp/invalid-store" <<'PY'
import pathlib, sqlite3, sys
db = sqlite3.connect(sys.argv[1])
state, digest = db.execute(
    "SELECT s.state,e.sha256 FROM snapshots s JOIN entries e ON e.snapshot_id=s.id WHERE s.id!='__store__'"
).fetchone()
assert state == "FAILED"
blob = pathlib.Path(sys.argv[2]) / "blobs" / "sha256" / digest[:2] / digest
assert not blob.exists()
db.close()
PY

# A hard-linked audit path must not change a file outside the store.
mkdir -p "$tmp/audit-link-source" "$tmp/audit-link-store"
printf 'audit link fixture\n' >"$tmp/audit-link-source/input.txt"
"$bin" init --source "$tmp/audit-link-source" --store "$tmp/audit-link-store"
printf 'protected audit sentinel\n' >"$tmp/audit-sentinel"
ln "$tmp/audit-sentinel" "$tmp/audit-link-store/audit/events.ndjson"
cp "$tmp/audit-sentinel" "$tmp/audit-sentinel.expected"
set +e
"$bin" snapshot --source "$tmp/audit-link-source" --store "$tmp/audit-link-store" >"$tmp/audit-link.out" 2>&1
rc=$?
set -e
[ "$rc" -ne 0 ]
cmp "$tmp/audit-sentinel" "$tmp/audit-sentinel.expected"
grep -q 'without hard links' "$tmp/audit-link.out"
python3 - "$tmp/audit-link-store/metadata.sqlite" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
assert db.execute("SELECT state FROM snapshots WHERE id!='__store__'").fetchone()[0] == "COMMITTED"
db.close()
PY

echo "SRE snapshot/verify/restore smoke: PASS"
