import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding='utf-8')
match = re.search(r'^define\b[^\n]*@"?__nsc_function_AtomicTls\$3A\$3A__Main"?\([^\n]*\{\n(.*?)^\}', text, re.M | re.S)
if not match:
    raise SystemExit('No se encontro el cuerpo NativeSharp')
body = match.group(1)
for symbol, width, alignment in (('atomic_unsupported_short', 16, 2), ('atomic_narrow_char', 8, 1), ('atomic_narrow_ushort', 16, 2)):
    addresses = {'@' + symbol}
    for line in body.splitlines():
        address = re.match(r'\s*(%[-\w.$]+)\s*=\s*call\b.*@llvm\.threadlocal\.address\.[^(]+\([^\n]*@' + symbol + r'\)', line)
        if address:
            addresses.add(address.group(1))
    for operation in ('load', 'store'):
        found = any(re.search(r'\b' + operation + r' atomic\b.*\bi' + str(width) + r'\b', line)
                    and re.search(r'\bseq_cst\b.*\balign ' + str(alignment) + r'\b', line)
                    and any(re.search(r'\bptr\s+(?:align\s+\d+\s+)?' + re.escape(address) + r'(?:\s|,)', line)
                            for address in addresses)
                    for line in body.splitlines())
        if not found:
            raise SystemExit('Falta ' + operation + ' atomico de ' + symbol)
print('IR TLS estrecho verificado')
