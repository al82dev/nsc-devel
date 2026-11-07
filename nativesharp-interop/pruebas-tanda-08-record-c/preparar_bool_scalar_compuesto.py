import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8')
target = 'volatile_bool_array[0]'
if source.count(target) != 129 or source.count('    return 0;') != 1:
    raise SystemExit('unexpected bool compound fixture; review scalar projection')
source = source.replace(target, 'volatile_bool_scalar')
source = source.replace('    return 0;',
    '    if (read_volatile_bool_scalar() != 0) return 62;\n    return 0;')
pathlib.Path(sys.argv[2]).write_text(source, encoding='utf-8')
