#!/usr/bin/env python3
import argparse,json,re,sys
from pathlib import Path
REQUIRED=('projects=5','graph-units=98','compiler-units=27','external-projects=4','status=source-verified')
OWNERS=('NativeSharpCore ','NativeSharpCoreCollectionsGeneric ','NativeSharpCoreIO ','NativeSharpLLVM_C ','NativeSharpCompilerNext ')
CHECKS={'core':41,'collections':23,'io':4,'llvm-c':3}
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  canonical=None; observed_resolutions=0
  for profile in ('o0','o1','o2','o3'):
   report=(a.output_dir/f'dependencies-{profile}.txt').read_text();graph=(a.output_dir/f'graph-{profile}.txt').read_text();modules=(a.output_dir/f'modules-{profile}.txt').read_text()
   if not all(x in report for x in REQUIRED): raise ValueError(f'{profile} dependency counts differ')
   match=re.search(r'^external-resolutions=(\d+)$',report,re.MULTILINE)
   if match is None or int(match.group(1)) <= 0: raise ValueError(f'{profile} has no cross-project resolutions')
   if observed_resolutions == 0: observed_resolutions=int(match.group(1))
   if not all(x in graph for x in OWNERS): raise ValueError(f'{profile} misses dependency owner')
   if '.obj' in graph or '.exe' in graph: raise ValueError(f'{profile} graph contains hidden binary input')
   checks=[]
   for name,units in CHECKS.items():
    check=(a.output_dir/f'check-{name}-{profile}.txt').read_text()
    expected=f'checked={units} failed=0'
    if expected not in check: raise ValueError(f'{profile} {name} source check does not contain {expected!r}')
    checks.append(check)
   current=(report,graph,modules,*checks)
   if canonical is None: canonical=current
   elif current!=canonical: raise ValueError(f'{profile} dependency graph differs')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh40-report','version':2,'gate':'SH40','status':'passed','projects':5,'units':98,'parsed_dependency_units':71,'external_resolutions':observed_resolutions,'problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_bootstrap_dependencies: error: {e}',file=sys.stderr);return 1
 print('SH40 source-only bootstrap dependency graph verified across O0-O3');return 0
if __name__=='__main__': raise SystemExit(main())
