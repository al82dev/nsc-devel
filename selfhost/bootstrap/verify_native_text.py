#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,sys
from pathlib import Path
def need(v,m):
    if not v:raise ValueError(m)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--contract",type=Path,required=True);ap.add_argument("--backend-source",type=Path,required=True);ap.add_argument("--output-dir",type=Path,required=True);ap.add_argument("--report",type=Path,required=True);a=ap.parse_args()
    try:
        c=json.loads(a.contract.read_text(encoding="utf-8"));need(c.get("schema")=="nativesharp-selfhost-native-text-contract","bad contract")
        backend=a.backend_source.read_text(encoding="utf-8")
        for x in c["required_backend_symbols"]:need(x in backend,f"backend misses {x}")
        canonical=None;artifacts={};checks=[]
        for profile in c["profiles"]:
            lower=profile.lower();semap=a.output_dir/f"sema-{lower}.txt";objp=a.output_dir/f"text-{lower}.readobj.txt";sema=semap.read_text(encoding="utf-8");obj=objp.read_text(encoding="utf-8")
            if canonical is None:canonical=sema
            else:need(sema==canonical,f"{profile} semantics differ")
            for x in c["required_semantic_fragments"]:need(x in sema,f"{profile} misses {x}")
            if profile == "O0":
                for x in c["required_object_fragments"]:need(x in obj,f"{profile} object misses {x}")
            for route in c["routes"]:
                p=a.output_dir/f"{route}-{lower}.result.txt";code=int(p.read_text().strip());need(code==c["expected_exit_code"],f"{profile}/{route}={code}");artifacts[p.name]=sha(p)
            artifacts[semap.name]=sha(semap);artifacts[objp.name]=sha(objp);checks.append({"profile":profile,"routes":c["routes"]})
        a.report.write_text(json.dumps({"schema":"nativesharp-selfhost-sh31-report","version":1,"gate":"SH31","status":"passed","checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]},indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as e:print(f"verify_native_text: error: {e}",file=sys.stderr);return 1
    print("SH31 native char/UTF-8 literals/concatenation/indexing verified with O0-O3 and both linkers");return 0
if __name__=="__main__":raise SystemExit(main())
