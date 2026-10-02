#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path
def need(v:bool,m:str)->None:
    if not v: raise ValueError(m)
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->int:
    ap=argparse.ArgumentParser(description="Verify SH30 NativeSharp object semantics.")
    ap.add_argument("--contract",type=Path,required=True);ap.add_argument("--output-dir",type=Path,required=True);ap.add_argument("--report",type=Path,required=True);a=ap.parse_args()
    try:
        c=json.loads(a.contract.read_text(encoding="utf-8"));need(c.get("schema")=="nativesharp-selfhost-object-model-contract","bad contract")
        canonical_symbols=None;canonical_sema=None;artifacts={};checks=[]
        for profile in c["profiles"]:
            lower=profile.lower();sp=a.output_dir/f"symbols-{lower}.txt";mp=a.output_dir/f"sema-{lower}.txt";op=a.output_dir/f"object-{lower}.readobj.txt"
            symbols=sp.read_text(encoding="utf-8");sema=mp.read_text(encoding="utf-8");obj=op.read_text(encoding="utf-8")
            if canonical_symbols is None:canonical_symbols=symbols;canonical_sema=sema
            else:need(symbols==canonical_symbols,f"{profile} symbols differ");need(sema==canonical_sema,f"{profile} semantics differ")
            for x in c["required_symbol_fragments"]:need(x in symbols,f"{profile} misses {x}")
            for x in c["required_semantic_fragments"]:need(x in sema,f"{profile} misses {x}")
            need(c["forbidden_semantic_fragment"] not in sema,f"{profile} contains error types")
            for name in c["required_native_functions"]:need(re.search(rf"__nativesharp_[0-9]+_{name}",obj)!=None,f"{profile} misses native {name}")
            for route in c["routes"]:
                p=a.output_dir/f"{route}-{lower}.result.txt";code=int(p.read_text().strip());need(code==c["expected_exit_code"],f"{profile}/{route}={code}");artifacts[p.name]=sha(p)
            for p in (sp,mp,op):artifacts[p.name]=sha(p)
            checks.append({"profile":profile,"routes":c["routes"]})
        a.report.write_text(json.dumps({"schema":"nativesharp-selfhost-sh30-report","version":1,"gate":"SH30","status":"passed","checks":checks,"artifacts":dict(sorted(artifacts.items())),"problems":[]},indent=2,sort_keys=True)+"\n",encoding="utf-8")
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as e:print(f"verify_object_model: error: {e}",file=sys.stderr);return 1
    print("SH30 this/base/member/property/indexer/operator model verified across O0-O3 and NSLink");return 0
if __name__=="__main__":raise SystemExit(main())
