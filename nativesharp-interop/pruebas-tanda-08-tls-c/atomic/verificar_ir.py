import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
mode = sys.argv[2]
name = r'entry' if mode == 'clang' else r'"?__nsc_function_AtomicTls\$3A\$3A__Main"?'
match = re.search(r'^define\b[^\n]*@' + name + r'\([^\n]*\{\n(.*?)^\}', text, re.M | re.S)
if not match:
    raise SystemExit('No se encontro la funcion esperada en el IR')
body = match.group(1)
addresses = {'@atomic_tls'}
for line in body.splitlines():
    result = re.match(r'\s*(%[-\w.$]+)\s*=\s*call\b.*@llvm\.threadlocal\.address\.[^(]+\([^\n]*@atomic_tls\)', line)
    if result:
        addresses.add(result.group(1))
seen = set()
for line in body.splitlines():
    for label, instruction in (('load', r'\bload atomic\b'), ('store', r'\bstore atomic\b'),
                               ('rmw', r'\batomicrmw(?: volatile)? add\b')):
        if not re.search(instruction, line) or not re.search(r'\bseq_cst\b.*\balign 4\b', line):
            continue
        if any(re.search(r'\bptr\s+(?:align\s+\d+\s+)?' + re.escape(address) + r'(?:\s|,)', line)
               for address in addresses):
            seen.add(label)
missing = {'load', 'store', 'rmw'} - seen
if missing:
    raise SystemExit('Faltan operaciones seq_cst sobre atomic_tls: ' + ', '.join(sorted(missing)))
print('IR atomico verificado para ' + mode)
