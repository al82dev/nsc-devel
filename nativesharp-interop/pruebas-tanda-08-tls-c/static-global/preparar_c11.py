import pathlib
import shutil

source = pathlib.Path(__file__).resolve().parent
dest = pathlib.Path(r'A:\nsc-interop-build\tls-abi\c11-fixture')
dest.mkdir(parents=True, exist_ok=True)
for path in source.iterdir():
    if path.is_file():
        shutil.copy2(path, dest / path.name)
for name in ('Api.c', 'Api.h'):
    path = dest / name
    text = path.read_text(encoding='utf-8')
    assert '__declspec(thread)' in text
    path.write_text(text.replace('__declspec(thread)', '_Thread_local'), encoding='utf-8')

header = dest / 'Api.h'
with header.open('a', encoding='utf-8') as stream:
    stream.write('\nint* tls_address(void);\n')
implementation = dest / 'Api.c'
with implementation.open('a', encoding='utf-8') as stream:
    stream.write('\nint* tls_address(void) { return &shared_tls; }\n')
consumer = dest / 'Main.ns'
text = consumer.read_text(encoding='utf-8')
before = '    shared_tls = 41;'
after = ('    int* address = &shared_tls;\n'
         '    if (address != tls_address()) return 5;\n'
         '    shared_tls = 41;\n'
         '    if (*address != 41) return 6;')
assert text.count(before) == 1
consumer.write_text(text.replace(before, after), encoding='utf-8')
