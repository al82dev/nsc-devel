#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,sys
from pathlib import Path
def need(v,m):
    if not v:raise ValueError(m)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--contract",type=Path,required=True);ap.add_argument("--output-dir",type=Path,required=True);ap.add_argument("--report",type=Path,required=True);a=ap.parse_args()
    try:
        c=json.loads(a.contract.read_text());need(c.get("schema")=="nativesharp-selfhost-arrays-memory-contract","bad contract");canonical=None;artifacts={};checks=[]
        for profile in c["profiles"]:
            lower=profile.lower();irp=a.output_dir/f"mir-{lower}.txt";op=a.output_dir/f"memory-{lower}.readobj.txt";ir=irp.read_text();obj=op.read_text()
            if canonical is None:canonical=ir
            else:need(ir==canonical,f"{profile} IR differs")
            for x in c["required_ir_fragments"]:need(x in ir,f"{profile} misses IR {x}")
            for x in c["required_imports"]:need(x in obj,f"{profile} misses import {x}")
            for route in c["routes"]:
                p=a.output_dir/f"{route}-{lower}.result.txt";code=int(p.read_text().strip());need(code==c["expected_exit_code"],f"{profile}/{route}={code}");artifacts[p.name]=sha(p)
            artifacts[irp.name]=sha(irp);artifacts[op.name]=sha(op);checks.append({"profile":profile,"routes":c["routes"]})
        a.report.write_text(json.dumps({"schema":"nativesharp-selfhost-sh32-report","version":1,"gate":"SH32","status":"passed","checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]},indent=2,sort_keys=True)+"\n")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as e:print(f"verify_arrays_memory: error: {e}",file=sys.stderr);return 1
    print("SH32 heap/stack arrays, indexed memory and delete[] verified across O0-O3 with NSLink");return 0
if __name__=="__main__":raise SystemExit(main())
