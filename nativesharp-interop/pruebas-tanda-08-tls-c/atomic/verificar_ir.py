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
for symbol, required, volatile in (
    ('atomic_tls', {'load', 'store', 'rmw'}, False),
    ('atomic_readonly', {'load'}, False),
    ('atomic_volatile', {'load', 'store', 'rmw'}, True),
):
    addresses = {'@' + symbol}
    for line in body.splitlines():
        result = re.match(r'\s*(%[-\w.$]+)\s*=\s*call\b.*@llvm\.threadlocal\.address\.[^(]+\([^\n]*@' + symbol + r'\)', line)
        if result:
            addresses.add(result.group(1))
    seen = set()
    for line in body.splitlines():
        for label, instruction in (('load', r'\bload atomic\b'), ('store', r'\bstore atomic\b'),
                                   ('rmw', r'\batomicrmw(?: volatile)? add\b')):
            if not re.search(instruction, line) or not re.search(r'\bseq_cst\b.*\balign 4\b', line):
                continue
            if volatile and not re.search(r'\bvolatile\b', line):
                continue
            if any(re.search(r'\bptr\s+(?:align\s+\d+\s+)?' + re.escape(address) + r'(?:\s|,)', line)
                   for address in addresses):
                seen.add(label)
    missing = required - seen
    if missing:
        raise SystemExit('Faltan operaciones seq_cst sobre ' + symbol + ': ' + ', '.join(sorted(missing)))
print('IR atomico verificado para ' + mode)
