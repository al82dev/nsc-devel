#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, sys
from pathlib import Path

def need(value: bool, message: str) -> None:
    if not value: raise ValueError(message)

def sha(path: Path) -> str: return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    ap = argparse.ArgumentParser(description="Verify SH28 whole-program validated IR.")
    ap.add_argument("--contract", type=Path, required=True)
    ap.add_argument("--ir-source", type=Path, required=True)
    ap.add_argument("--backend-source", type=Path, required=True)
    ap.add_argument("--driver-source", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    a = ap.parse_args()
    try:
        c = json.loads(a.contract.read_text(encoding="utf-8"))
        need(c.get("schema") == "nativesharp-selfhost-program-ir-contract", "bad contract")
        ir = a.ir_source.read_text(encoding="utf-8")
        backend = a.backend_source.read_text(encoding="utf-8")
        driver = a.driver_source.read_text(encoding="utf-8")
        for symbol in c["required_ir_symbols"]: need(symbol in ir, f"IR misses {symbol}")
        need(c["required_backend_signature"] in backend, "backend does not require validated IR")
        need(c["forbidden_backend_signature"] not in backend, "raw syntax backend entry remains")
        need(driver.count("IrPipeline.Prepare(tree, semantic)") >= 2, "driver bypasses IR admission")
        artifacts, checks = {}, []
        for stage in c["stages"]:
            canonical = None
            for profile in c["profiles"]:
                path = a.output_dir / f"{stage}-{profile.lower()}.txt"
                text = path.read_text(encoding="utf-8")
                need(text.startswith(f"{stage}.module schema=1\n") or
                     (stage == "dominance" and text.startswith("dominance.module schema=1\n")),
                     f"bad {stage}/{profile} header")
                if canonical is None: canonical = text
                else: need(text == canonical, f"{stage}/{profile} differs from O0")
                artifacts[path.name] = sha(path)
            if stage in ("mir", "cfg", "dominance", "ssa"):
                for name in c["expected_functions"]:
                    need(f"function @{name}" in canonical, f"{stage} misses {name}")
            checks.append({"stage": stage, "sha256": hashlib.sha256(canonical.encode()).hexdigest()})
        report = {"schema":"nativesharp-selfhost-sh28-report","version":1,"gate":"SH28",
                  "status":"passed","profiles":c["profiles"],"stages":c["stages"],
                  "checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]}
        a.report.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as e:
        print(f"verify_program_ir: error: {e}", file=sys.stderr); return 1
    print("SH28 whole-program HIR/MIR/CFG/dominance/SSA verified across O0-O3")
    return 0
if __name__ == "__main__": raise SystemExit(main())
