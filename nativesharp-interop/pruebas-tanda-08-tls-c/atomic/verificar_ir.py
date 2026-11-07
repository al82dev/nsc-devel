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
    ('atomic_tls', {'load', 'store', 'rmw', 'and', 'or', 'xor', 'cmpxchg'}, False),
    ('atomic_readonly', {'load'}, False),
    ('atomic_volatile', {'load', 'store', 'rmw'}, True),
    ('atomic_wide', {'load', 'store', 'rmw', 'cmpxchg'}, False),
    ('atomic_unsigned', {'store', 'rmw', 'cmpxchg'}, False),
    ('atomic_unsigned_wide', {'store', 'rmw', 'cmpxchg'}, False),
    ('atomic_float', {'load', 'store'}, False),
    ('atomic_double', {'load', 'store'}, True),
    ('atomic_float_const', {'load'}, False),
):
    addresses = {'@' + symbol}
    for line in body.splitlines():
        result = re.match(r'\s*(%[-\w.$]+)\s*=\s*call\b.*@llvm\.threadlocal\.address\.[^(]+\([^\n]*@' + symbol + r'\)', line)
        if result:
            addresses.add(result.group(1))
    seen = set()
    compare_exchanges = 0
    for line in body.splitlines():
        for label, instruction in (('load', r'\bload atomic\b'), ('store', r'\bstore atomic\b'),
                                   ('rmw', r'\batomicrmw(?: volatile)? add\b'),
                                   ('and', r'\batomicrmw(?: volatile)? and\b'),
                                   ('or', r'\batomicrmw(?: volatile)? or\b'),
                                   ('xor', r'\batomicrmw(?: volatile)? xor\b'),
                                   ('cmpxchg', r'\bcmpxchg(?: volatile)?\b')):
            align = 8 if symbol in {'atomic_wide', 'atomic_unsigned_wide', 'atomic_double'} else 4
            if not re.search(instruction, line) or not re.search(r'\bseq_cst\b.*\balign ' + str(align) + r'\b', line):
                continue
            if label == 'cmpxchg' and not re.search(r'\bseq_cst\s+seq_cst\b', line):
                continue
            # Clang 23 emits atomicrmw without LLVM volatile for this C operator;
            # loads and stores still require the volatile marker.
            if volatile and label in {'load', 'store'} and not re.search(r'\bvolatile\b', line):
                continue
            if any(re.search(r'\bptr\s+(?:align\s+\d+\s+)?' + re.escape(address) + r'(?:\s|,)', line)
                   for address in addresses):
                seen.add(label)
                if label == 'cmpxchg':
                    compare_exchanges += 1
    missing = required - seen
    if missing:
        raise SystemExit('Faltan operaciones seq_cst sobre ' + symbol + ': ' + ', '.join(sorted(missing)))
    if symbol == 'atomic_tls' and compare_exchanges < 4:
        raise SystemExit('Faltan intercambios atomicos para multiplicacion y desplazamientos')
    if symbol == 'atomic_volatile' and compare_exchanges < 4:
        raise SystemExit('Faltan intercambios atomicos para el objeto volatile')
    if symbol == 'atomic_volatile' and mode == 'native':
        volatile_cas = [line for line in body.splitlines()
                        if re.search(r'\bcmpxchg volatile\b', line)
                        and any(re.search(r'\bptr\s+' + re.escape(address) + r'(?:\s|,)', line)
                                for address in addresses)]
        if len(volatile_cas) < 4:
            raise SystemExit('Falta volatile en los intercambios atomicos')
print('IR atomico verificado para ' + mode)
