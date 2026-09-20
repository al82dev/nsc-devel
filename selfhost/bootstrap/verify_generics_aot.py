#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,sys
from pathlib import Path
def need(value,message):
    if not value: raise ValueError(message)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument("--contract",type=Path,required=True);parser.add_argument("--output-dir",type=Path,required=True);parser.add_argument("--report",type=Path,required=True);args=parser.parse_args()
    try:
        contract=json.loads(args.contract.read_text());need(contract.get("schema")=="nativesharp-selfhost-generics-aot-contract","bad contract");artifacts={};checks=[]
        for profile in contract["profiles"]:
            lower=profile.lower();registry=args.output_dir/f"generics-{lower}.txt";need(registry.read_text()==contract["expected_registry"],f"{profile} registry differs");artifacts[registry.name]=sha(registry)
            for route in contract["routes"]:
                result=args.output_dir/f"{route}-{lower}.result.txt";need(int(result.read_text().strip())==contract["expected_exit_code"],f"{profile}/{route} failed");artifacts[result.name]=sha(result)
            checks.append({"profile":profile,"routes":contract["routes"]})
        args.report.write_text(json.dumps({"schema":"nativesharp-selfhost-sh34-report","version":1,"gate":"SH34","status":"passed","checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]},indent=2,sort_keys=True)+"\n")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as error: print(f"verify_generics_aot: error: {error}",file=sys.stderr);return 1
    print("SH34 canonical generic AOT identity verified across O0-O3 with both linkers");return 0
if __name__=="__main__": raise SystemExit(main())
