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

echo "SRE snapshot/verify/restore smoke: PASS"
