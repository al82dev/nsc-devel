#!/usr/bin/env python3
import argparse,json,sys
from pathlib import Path
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  for profile in ('o0','o1','o2','o3'):
   text=(a.output_dir/f'mir-{profile}.txt').read_text()
   for fragment in ('ownership.move','cleanup.if-live @second','cleanup.if-live @first','free.array'):
    if fragment not in text: raise ValueError(f'{profile} misses {fragment}')
   for route in ('nslink',):
    if int((a.output_dir/f'{route}-{profile}.result.txt').read_text())!=42: raise ValueError(f'{route}/{profile} failed')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh35-report','version':1,'gate':'SH35','status':'passed','problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_ownership_cleanup: error: {e}',file=sys.stderr);return 1
 print('SH35 owned/move/release and exit cleanup verified across O0-O3 with NSLink');return 0
if __name__=='__main__': raise SystemExit(main())
