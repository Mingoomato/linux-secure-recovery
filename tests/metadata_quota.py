#!/usr/bin/env python3
"""Exercise real directory restoration and persistent, deduplicated blob quotas."""
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())

def run(*args, expected=0):
    result = subprocess.run([binary, *map(str, args)], text=True, capture_output=True, timeout=20)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(prefix="sre-metadata-") as tmp:
    base = Path(tmp)
    source, store, target = (base / name for name in ("source", "store", "target"))
    (source / "nested" / "empty").mkdir(parents=True)
    target.mkdir()
    (source / "nested" / "data").write_bytes(b"original")
    timestamp = 1700000000123456789
    for path, mode in ((source / "nested", 0o750), (source / "nested" / "empty", 0o710)):
        path.chmod(mode)
        os.utime(path, ns=(timestamp, timestamp))
    run("init", "--source", source, "--store", store, "--max-store-bytes", "8")
    snapshot_output = run("snapshot", "--source", source, "--store", store)
    snapshot = snapshot_output.split()[0]
    assert "blob_bytes=8" in snapshot_output
    manifest = json.loads((store / "manifests" / f"{snapshot}.json").read_text())
    assert manifest["entry_count"] == 1
    assert manifest["entries"][0]["relative_path"] == "nested/data"
    assert manifest["entries"][0]["observed_at_ns"] > 0
    directory_records = {item["relative_path"]: item for item in manifest["directories"]}
    assert set(directory_records) == {"nested", "nested/empty"}
    assert directory_records["nested"]["mode"] == 0o750
    assert directory_records["nested/empty"]["mtime_ns"] == timestamp
    plan = [json.loads(line) for line in run(
        "restore", "--store", store, "--snapshot", snapshot, "--target", target, "--dry-run"
    ).splitlines()]
    planned_directories = {item["path"]: item for item in plan
                           if item["type"] == "restore_plan_directory"}
    assert set(planned_directories) == {"nested", "nested/empty"}
    assert all(item["action"] == "create" for item in planned_directories.values())
    summary = next(item for item in plan if item["type"] == "restore_plan_summary")
    assert summary["directory_count"] == summary["directories_to_create"] == 2
    duplicate = run("snapshot", "--source", source, "--store", store)
    assert "blob_bytes=8" in duplicate
    run("restore", "--store", store, "--snapshot", snapshot, "--target", target, "--confirm")
    assert (target / "nested" / "data").read_bytes() == b"original"
    for relative in ("nested", "nested/empty"):
        before, after = (source / relative).stat(), (target / relative).stat()
        assert stat.S_IMODE(before.st_mode) == stat.S_IMODE(after.st_mode)
        assert (before.st_uid, before.st_gid, before.st_mtime_ns) == (after.st_uid, after.st_gid, after.st_mtime_ns)
    (source / "extra").write_bytes(b"new")
    failed = subprocess.run([binary, "snapshot", "--source", str(source), "--store", str(store)],
                            text=True, capture_output=True, timeout=20)
    assert failed.returncode != 0 and "SRE_STORE_QUOTA_EXCEEDED" in failed.stderr
    run("verify", "--store", store, "--snapshot", snapshot)
    (source / "extra").unlink()
    # Lowering the cap does not invalidate existing snapshots or deduplicated content.
    run("init", "--source", source, "--store", store, "--max-store-bytes", "1")
    run("snapshot", "--source", source, "--store", store)
    run("init", "--source", source, "--store", store, "--max-store-bytes", "0")
    (source / "extra").write_bytes(b"new")
    assert "blob_bytes=11" in run("snapshot", "--source", source, "--store", store)
    audit = [json.loads(line) for line in (store / "audit/events.ndjson").read_text().splitlines()]
    restored = next(row for row in audit if row["operation"] == "restore")
    assert restored["affected_count"] == 1 and restored["directory_count"] == 2
print("directory metadata and persistent blob quota: PASS")
