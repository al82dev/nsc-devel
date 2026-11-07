import pathlib
import sys

lines = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8').splitlines()
loads = [line for line in lines if 'c.bool.compound.load' in line and ' = load ' in line]
stores = [line for line in lines if 'store atomic volatile i8 %c.bool.compound.bits' in line]
if len(loads) != 16 or len(stores) != 16:
    raise SystemExit(f'expected sixteen C bool compound loads and stores, got {len(loads)} and {len(stores)}')
if any('load atomic volatile i8' not in line or ' acquire, align 1' not in line for line in loads):
    raise SystemExit('C bool compound load is missing volatile acquire or byte alignment')
if any(' release, align 1' not in line for line in stores):
    raise SystemExit('C bool compound store is missing volatile release or byte alignment')
promotions = [line for line in lines if 'c.bool.compound.promoted' in line and ' = zext i1 ' in line]
normalizations = [line for line in lines if 'c.bool.compound.normalized' in line and ' = icmp ne i32 ' in line]
if len(promotions) != 11 or len(normalizations) != 11:
    raise SystemExit(f'expected eleven i32 promotions and normalizations, got {len(promotions)} and {len(normalizations)}')
for instruction in (' sdiv i32 ', ' srem i32 ', ' shl i32 ', ' ashr i32 '):
    if not any(instruction in line for line in lines):
        raise SystemExit(f'missing C bool compound instruction: {instruction.strip()}')
print('C volatile bool compound IR: sixteen byte loads/stores and eleven i32 calculations.')
