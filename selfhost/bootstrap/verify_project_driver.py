#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, sys
from pathlib import Path

def digest(path: Path) -> str: return hashlib.sha256(path.read_bytes()).hexdigest()
def need(value: bool, message: str) -> None:
    if not value: raise ValueError(message)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        c = json.loads(args.contract.read_text(encoding="utf-8"))
        need(c.get("schema") == "nativesharp-selfhost-project-driver-contract", "bad contract schema")
        need(c["optimizations"] == ["O0", "O1", "O2", "O3"], "incomplete profiles")
        need(len(c["work_items"]) == 14, "incomplete work-item ledger")
        hashes: dict[str, str] = {}; dumps: list[bytes] = []; checks = 0
        for profile in c["optimizations"]:
            lower = profile.lower(); dump = args.output_dir / f"project-{lower}.txt"
            obj = args.output_dir / f"project-{lower}.obj"; metadata = args.output_dir / f"project-{lower}.nsmeta"
            manifest = args.output_dir / f"project-{lower}.manifest"
            for path in (dump, obj, metadata, manifest):
                need(path.is_file() and path.stat().st_size > 0, f"missing {path.name}")
                hashes[path.name] = digest(path)
            text = dump.read_text(encoding="utf-8")
            for invariant in (f"name={c['expected_project']}", f"output={c['expected_output']}",
                              f"sdk={c['expected_sdk']}", f"sources={c['expected_sources']}",
                              f"references={c['expected_references']}"):
                need(invariant in text, f"{profile} dump misses {invariant}"); checks += 1
            dumps.append(dump.read_bytes())
            need(metadata.read_text(encoding="utf-8").startswith("NSMETA/2\n"), "bad nsmeta")
            need("target=x86_64-pc-windows-msvc" in manifest.read_text(encoding="utf-8"), "bad manifest")
            checks += 2
            for route in ("nslink",):
                exe = args.output_dir / f"{route}-{lower}.exe"
                need(exe.is_file(), f"missing {exe.name}"); hashes[exe.name] = digest(exe); checks += 1
        need(all(value == dumps[0] for value in dumps[1:]), "project dump differs across profiles")
        rsp = args.output_dir / "driver.rsp"; rsp_dump = args.output_dir / "response-project.txt"
        need(rsp.is_file() and rsp_dump.read_bytes() == dumps[0], "response-file precedence differs")
        report = {"schema":"nativesharp-selfhost-sh09-report","version":1,"status":"passed",
                  "profiles":4,"linkers":len(c["linkers"]),"project_checks":checks,"response_files":True,
                  "metadata_compatibility":"N-1,N","work_items":c["work_items"],
                  "artifacts":dict(sorted(hashes.items()))}
        args.report.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verify_project_driver: error: {error}", file=sys.stderr); return 1
    print("SH09 project/driver verified: 4 profiles, response file, metadata, manifests, NSLink")
    return 0
if __name__ == "__main__": raise SystemExit(main())
