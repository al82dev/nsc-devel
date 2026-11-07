import pathlib
import sys

lines = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8').splitlines()
loads = [line for line in lines if 'c.bool.compound.load' in line and ' = load ' in line]
stores = [line for line in lines if 'store atomic volatile i8 %c.bool.compound.bits' in line]
if len(loads) != 59 or len(stores) != 59:
    raise SystemExit(f'expected 59 C bool compound loads and stores, got {len(loads)} and {len(stores)}')
if any('load atomic volatile i8' not in line or ' acquire, align 1' not in line for line in loads):
    raise SystemExit('C bool compound load is missing volatile acquire or byte alignment')
if any(' release, align 1' not in line for line in stores):
    raise SystemExit('C bool compound store is missing volatile release or byte alignment')
promotions = [line for line in lines if 'c.bool.compound.promoted' in line and ' = zext i1 ' in line]
normalizations = [line for line in lines if 'c.bool.compound.normalized' in line and ' = icmp ne i32 ' in line]
promotions64 = [line for line in lines if 'c.bool.compound.promoted' in line and ' = zext i1 ' in line and ' to i64' in line]
normalizations64 = [line for line in lines if 'c.bool.compound.normalized' in line and ' = icmp ne i64 ' in line]
promotions32 = [line for line in promotions if ' to i32' in line]
if len(promotions32) != 30 or len(normalizations) != 30 or len(promotions64) != 22 or len(normalizations64) != 22:
    raise SystemExit(f'unexpected i32/i64 promotions or normalizations: {len(promotions32)}, {len(normalizations)}, {len(promotions64)}, {len(normalizations64)}')
for instruction in (' sdiv i32 ', ' srem i32 ', ' udiv i32 ', ' urem i32 ', ' sdiv i64 ', ' srem i64 ', ' udiv i64 ', ' urem i64 ', ' shl i32 ', ' ashr i32 '):
    if not any(instruction in line for line in lines):
        raise SystemExit(f'missing C bool compound instruction: {instruction.strip()}')
print('C volatile bool compound IR: 59 byte loads/stores and i32/i64 calculations.')
