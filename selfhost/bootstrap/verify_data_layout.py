#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path

def need(v: bool, m: str) -> None:
    if not v: raise ValueError(m)
def sha(p: Path) -> str: return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> int:
    ap=argparse.ArgumentParser(description="Verify SH29 data/layout and global codegen.")
    ap.add_argument("--contract",type=Path,required=True); ap.add_argument("--output-dir",type=Path,required=True)
    ap.add_argument("--report",type=Path,required=True); a=ap.parse_args()
    try:
        c=json.loads(a.contract.read_text(encoding="utf-8")); need(c.get("schema")=="nativesharp-selfhost-data-layout-contract","bad contract")
        canonical=None; artifacts={}; checks=[]
        for profile in c["profiles"]:
            lower=profile.lower(); layout=a.output_dir/f"layout-{lower}.txt"; inspection=a.output_dir/f"data-{lower}.readobj.txt"
            text=layout.read_text(encoding="utf-8"); obj=inspection.read_text(encoding="utf-8")
            if canonical is None: canonical=text
            else: need(text==canonical,f"{profile} layout differs from O0")
            for fragment in c["required_layout_fragments"]: need(fragment in text,f"{profile} misses layout: {fragment}")
            for name in c["required_global_names"]:
                need(re.search(rf"__nativesharp_global_[0-9]+_{name}",obj) is not None,f"{profile} misses global {name}")
            need(c["required_global_relocation"] in obj,f"{profile} has no RIP-relative global relocation")
            need(c["forbidden_global_relocation"] not in obj,f"{profile} emits an image-base-truncating relocation")
            for route in c["routes"]:
                result=a.output_dir/f"{route}-{lower}.result.txt"; code=int(result.read_text(encoding="ascii").strip())
                need(code==c["expected_exit_code"],f"{profile}/{route} returned {code}"); artifacts[result.name]=sha(result)
            artifacts[layout.name]=sha(layout); artifacts[inspection.name]=sha(inspection)
            checks.append({"profile":profile,"layout_sha256":sha(layout),"routes":c["routes"]})
        report={"schema":"nativesharp-selfhost-sh29-report","version":1,"gate":"SH29","status":"passed",
                "profiles":c["profiles"],"checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]}
        a.report.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as e:
        print(f"verify_data_layout: error: {e}",file=sys.stderr); return 1
    print("SH29 layouts, static globals and Win64 relocations verified with NSLink")
    return 0
if __name__=="__main__": raise SystemExit(main())
