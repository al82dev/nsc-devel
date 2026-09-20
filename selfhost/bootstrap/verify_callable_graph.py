#!/usr/bin/env python3
import argparse,json,sys
from pathlib import Path
EXPECTED='callables=5\ncall Increment\ndelegate Transform\nlambda expression\noverride Apply\nvirtual Apply\n'
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  for profile in ('o0','o1','o2','o3'):
   if (a.output_dir/f'callables-{profile}.txt').read_text()!=EXPECTED: raise ValueError(f'{profile} graph differs')
   for route in ('lld','nslink'):
    if int((a.output_dir/f'{route}-{profile}.result.txt').read_text())!=42: raise ValueError(f'{route}/{profile} failed')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh36-report','version':1,'gate':'SH36','status':'passed','problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_callable_graph: error: {e}',file=sys.stderr);return 1
 print('SH36 callable graph verified across O0-O3 with both linkers');return 0
if __name__=='__main__': raise SystemExit(main())
