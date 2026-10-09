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
"$bin" verify --store "$tmp/store" --snapshot "$snapshot_id" >"$tmp/verify.json"
python3 - "$tmp/verify.json" <<'PY'
import json, sys
result = json.load(open(sys.argv[1], encoding="utf-8"))
assert result["bad"] == 0 and result["verified"] == 2
PY

"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-dry" --dry-run >"$tmp/dry.json"
[ ! -e "$tmp/target-dry/nested/data.txt" ]
printf 'existing bytes\n' >"$tmp/target-conflict/nested/data.txt"
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-conflict" --confirm >"$tmp/conflict.json"
grep -q 'existing bytes' "$tmp/target-conflict/nested/data.txt"
python3 - "$tmp/conflict.json" <<'PY'
import json, sys
result = json.loads(open(sys.argv[1], encoding="utf-8").read().splitlines()[-1])
assert result["conflicts"] == 1 and result["restored"] == 1
PY
"$bin" restore --store "$tmp/store" --snapshot "$snapshot_id" --target "$tmp/target-clean" --confirm >"$tmp/restore.json"
cmp "$tmp/source/nested/data.txt" "$tmp/target-clean/nested/data.txt"
cmp "$tmp/source/nested/back\\slash.txt" "$tmp/target-clean/nested/back\\slash.txt"
[ ! -e "$tmp/target-clean/external-link" ]

# Simulate process death after the durable COPYING row, before its entry transaction.
printf 'stale temporary bytes\n' >"$tmp/store/temp/stale-crash.part"
ln -s "$tmp/external.txt" "$tmp/store/temp/stale-link.part"
python3 - "$tmp/store/metadata.sqlite" <<'PY'
import sqlite3, sys, time
db = sqlite3.connect(sys.argv[1])
db.execute("INSERT INTO snapshots(id,source_root,created_at,state,incomplete_count) VALUES(?,?,?,?,?)",
           ("stale-crash", "/synthetic/source", int(time.time()), "COPYING", 0))
db.commit()
db.close()
PY
"$bin" doctor --store "$tmp/store" >"$tmp/doctor.json"
python3 - "$tmp/doctor.json" "$tmp/store/metadata.sqlite" <<'PY'
import json, sqlite3, sys
result = json.load(open(sys.argv[1], encoding="utf-8"))
assert result["recovered_incomplete"] == 1
assert result["removed_temp_files"] == 2
db = sqlite3.connect(sys.argv[2])
state, incomplete = db.execute("SELECT state,incomplete_count FROM snapshots WHERE id='stale-crash'").fetchone()
assert (state, incomplete) == ("FAILED", 1)
PY
[ ! -e "$tmp/store/temp/stale-crash.part" ]
[ ! -e "$tmp/store/temp/stale-link.part" ]
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

echo "SRE snapshot/verify/restore smoke: PASS"
