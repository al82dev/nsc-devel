#!/usr/bin/env python3
import argparse,json,sys
from pathlib import Path
KINDS={'exe':('kind=Exe','product=DriverExe.exe','link=true'),'library':('kind=Library','product=DriverLibrary.dll','link=true'),'staticlibrary':('kind=StaticLibrary','product=DriverStatic.lib','link=true'),'facade':('kind=Facade','product=DriverFacade.nsmeta','link=false')}
def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 try:
  for profile in ('o0','o1','o2','o3'):
   for kind,parts in KINDS.items():
    text=(a.output_dir/f'{kind}-{profile}.plan').read_text()
    if not all(part in text for part in parts): raise ValueError(f'{kind}/{profile} plan differs')
   if 'output=Exe' not in (a.output_dir/f'response-{profile}.txt').read_text(): raise ValueError(f'{profile} response failed')
   for route in ('nslink',):
    if int((a.output_dir/f'{route}-{profile}.result.txt').read_text())!=42: raise ValueError(f'{route}/{profile} failed')
  a.report.write_text(json.dumps({'schema':'nativesharp-selfhost-sh39-report','version':1,'gate':'SH39','status':'passed','problems':[]},indent=2)+'\n')
 except (OSError,ValueError) as e: print(f'verify_driver_products: error: {e}',file=sys.stderr);return 1
 print('SH39 EXE/DLL/LIB/facade plans, response files and executable links verified');return 0
if __name__=='__main__': raise SystemExit(main())
