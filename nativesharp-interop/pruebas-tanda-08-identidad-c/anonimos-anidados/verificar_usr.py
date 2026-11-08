from pathlib import Path
import re
import sys

log = Path(sys.argv[1]).read_text(encoding='utf-8')
data = Path(sys.argv[2]).read_bytes()
wanted = ['@FI@mode', '@FI@value', '@AnonFirst', '@AnonSecond']
usrs = set(re.findall(r'c:@S@AnonOuter\S+', log))
for suffix in wanted:
    matches = [usr for usr in usrs if usr.endswith(suffix)]
    if len(matches) != 1 or matches[0].encode('ascii') not in data:
        print('USR ausente o ambiguo:', suffix)
        raise SystemExit(1)
    print('USR publicado:', matches[0])
