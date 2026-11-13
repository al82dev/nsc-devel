"""Exercise checked conversions from signed and unsigned 128-bit integers."""

import sys

import checked_i128_long as runner


TARGETS = (('sbyte', 8, True), ('byte', 8, False), ('char', 8, False),
           ('short', 16, True), ('ushort', 16, False), ('int', 32, True),
           ('uint', 32, False), ('long', 64, True), ('ulong', 64, False),
           ('nint', 64, True), ('nuint', 64, False),
           ('i128', 128, True), ('u128', 128, False))


def add(name, source, target, expression, expected, checked=True, comparison=None):
    cast = f'({target})value'
    if checked:
        cast = f'checked({cast})'
    comparison = comparison or f'({source})result == value'
    program = f'''{target} Convert({source} value) {{ return {cast}; }}
int Main() {{ {source} value = {expression}; {target} result = Convert(value);
 return {comparison} ? 42 : 7; }}'''
    runner.CASES[name] = (program, expected)


def main():
    runner.CASES = {}
    for source in ('i128', 'u128'):
        for target, width, signed in TARGETS:
            prefix = source + '-' + target + '-'
            add(prefix + 'zero', source, target, f'({source})0', 42)
            add(prefix + 'one', source, target, f'({source})1', 42)
            if width < 128:
                limit_bits = width - 1 if signed else width
                edge = f'(({source})1 << {limit_bits})'
                add(prefix + 'max', source, target, edge + f' - ({source})1', 42)
                add(prefix + 'above', source, target, edge, 147)
                if source == 'i128':
                    if signed:
                        add(prefix + 'min', source, target, '-' + edge, 42)
                        add(prefix + 'below', source, target, '-' + edge + ' - (i128)1', 147)
                    else:
                        add(prefix + 'below', source, target, '(i128)-1', 147)
                if source == 'u128' and signed:
                    add(prefix + 'highbits', source, target, '(u128)-1', 147)
                wrap = f'(({source})1 << {width})'
                add(prefix + 'ordinary', source, target, wrap, 42, False,
                    f'result == ({target})0')
            elif source != target:
                if source == 'i128':
                    add(prefix + 'below', source, target, '(i128)-1', 147)
                    add(prefix + 'ordinary', source, target, '(i128)-1', 42, False,
                        'result == (u128)-1')
                else:
                    edge = '((u128)1 << 127)'
                    add(prefix + 'max', source, target, edge + ' - (u128)1', 42)
                    add(prefix + 'above', source, target, edge, 147)
                    add(prefix + 'highbits', source, target, '(u128)-1', 147)
                    add(prefix + 'ordinary', source, target, edge, 42, False,
                        'result < (i128)0')
    return runner.main()


if __name__ == '__main__':
    sys.exit(main())
