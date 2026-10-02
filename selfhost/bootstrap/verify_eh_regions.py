#!/usr/bin/env python3
import argparse,json,sys
from pathlib import Path
EXPECTED='eh-regions schema=1\ntry=1 catch=0 finally=1 throw=0 using=0\n'
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  for profile in ('o0','o1','o2','o3'):
   if (a.output_dir/f'eh-{profile}.txt').read_text()!=EXPECTED: raise ValueError(f'{profile} EH graph differs')
   if 'UnwindInformation [' not in (a.output_dir/f'unwind-{profile}.txt').read_text(): raise ValueError(f'{profile} lacks unwind metadata')
   for route in ('nslink',):
    if int((a.output_dir/f'{route}-{profile}.result.txt').read_text())!=42: raise ValueError(f'{route}/{profile} failed')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh38-report','version':1,'gate':'SH38','status':'passed','problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_eh_regions: error: {e}',file=sys.stderr);return 1
 print('SH38 EH regions and Win64 unwind metadata verified across O0-O3 with NSLink');return 0
if __name__=='__main__': raise SystemExit(main())
