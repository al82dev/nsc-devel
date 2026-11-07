import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding='utf-8')
match = re.search(r'^define\b[^\n]*@"?__nsc_function_AtomicTls\$3A\$3A__Main"?\([^\n]*\{\n(.*?)^\}', text, re.M | re.S)
if not match:
    raise SystemExit('No se encontro el cuerpo NativeSharp')
body = match.group(1)
for symbol, volatile in (('atomic_unsupported_short', False), ('atomic_narrow_volatile', True)):
    addresses = {'@' + symbol}
    for line in body.splitlines():
        address = re.match(r'\s*(%[-\w.$]+)\s*=\s*call\b.*@llvm\.threadlocal\.address\.[^(]+\([^\n]*@' + symbol + r'\)', line)
        if address:
            addresses.add(address.group(1))
    updates = [line for line in body.splitlines()
               if re.search(r'\bcmpxchg' + (r' volatile' if volatile else r'(?: volatile)?') + r'\b', line)
               and re.search(r'\bseq_cst\s+seq_cst\b.*\balign 2\b', line)
               and any(re.search(r'\bptr\s+' + re.escape(address) + r'(?:\s|,)', line) for address in addresses)]
    if len(updates) < 4:
        raise SystemExit('Faltan cuatro intercambios atomicos para ' + symbol)
print('IR de incrementos estrechos verificado')
