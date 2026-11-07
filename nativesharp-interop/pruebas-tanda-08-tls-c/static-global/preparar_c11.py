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
