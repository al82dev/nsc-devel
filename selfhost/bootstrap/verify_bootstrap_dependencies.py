#!/usr/bin/env python3
import argparse,json,sys
from pathlib import Path
REQUIRED=('projects=5','graph-units=96','compiler-units=26','external-projects=4','external-resolutions=3269','status=source-verified')
OWNERS=('NativeSharpCore ','NativeSharpCoreCollectionsGeneric ','NativeSharpCoreIO ','NativeSharpLLVM_C ','NativeSharpCompilerNext ')
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  canonical=None
  for profile in ('o0','o1','o2','o3'):
   report=(a.output_dir/f'dependencies-{profile}.txt').read_text();graph=(a.output_dir/f'graph-{profile}.txt').read_text();modules=(a.output_dir/f'modules-{profile}.txt').read_text()
   if not all(x in report for x in REQUIRED): raise ValueError(f'{profile} dependency counts differ')
   if not all(x in graph for x in OWNERS): raise ValueError(f'{profile} misses dependency owner')
   if '.obj' in graph or '.exe' in graph: raise ValueError(f'{profile} graph contains hidden binary input')
   current=(report,graph,modules)
   if canonical is None: canonical=current
   elif current!=canonical: raise ValueError(f'{profile} dependency graph differs')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh40-report','version':1,'gate':'SH40','status':'passed','projects':5,'units':96,'problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_bootstrap_dependencies: error: {e}',file=sys.stderr);return 1
 print('SH40 source-only bootstrap dependency graph verified across O0-O3');return 0
if __name__=='__main__': raise SystemExit(main())
