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
        contract=json.loads(args.contract.read_text());need(contract.get("schema")=="nativesharp-selfhost-interop-base-contract","bad contract");canonical=None;artifacts={};checks=[]
        for profile in contract["profiles"]:
            lower=profile.lower();ast_path=args.output_dir/f"ast-{lower}.txt";object_path=args.output_dir/f"interop-{lower}.readobj.txt";ast=ast_path.read_text();obj=object_path.read_text()
            if canonical is None: canonical=ast
            else: need(ast==canonical,f"{profile} AST differs")
            for fragment in contract["required_ast_fragments"]: need(fragment in ast,f"{profile} misses AST {fragment}")
            for fragment in contract["required_object_fragments"]: need(fragment in obj,f"{profile} misses object {fragment}")
            for route in contract["routes"]:
                result=args.output_dir/f"{route}-{lower}.result.txt";need(int(result.read_text().strip())==contract["expected_exit_code"],f"{profile}/{route} failed");artifacts[result.name]=sha(result)
            artifacts[ast_path.name]=sha(ast_path);artifacts[object_path.name]=sha(object_path);checks.append({"profile":profile,"routes":contract["routes"]})
        args.report.write_text(json.dumps({"schema":"nativesharp-selfhost-sh33-report","version":1,"gate":"SH33","status":"passed","checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]},indent=2,sort_keys=True)+"\n")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as error: print(f"verify_interop_base: error: {error}",file=sys.stderr);return 1
    print("SH33 ref/out/in, extern C and basic varargs verified across O0-O3 with both linkers");return 0
if __name__=="__main__": raise SystemExit(main())
