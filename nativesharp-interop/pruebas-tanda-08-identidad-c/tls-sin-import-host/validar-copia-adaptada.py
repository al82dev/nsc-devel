"""T08-D04b: RuntimeTls="ImportDirectory" distributes an adapted copy of a C TLS DLL without import directory.

The Windows loader initializes the static TLS of a DLL only when the image has an import directory (validar.py).
With the explicit, per-reference opt-in RuntimeTls="ImportDirectory", NSC derives a copy whose import directory is
one null descriptor placed in the zero padding that follows the contents of a read-only data section, and uses that
copy as the Runtime; the original is never written. Checks:
1. direct, O0-O3: the consumer reads the initial TLS value 7; the original DLL keeps its bytes; the copy differs
   only in the section VirtualSize, DataDirectory[1] and CheckSum; its import directory points to 20 zero bytes;
2. CheckSum: an original with a CheckSum (/release) gets the recomputed PE checksum; one without keeps 0;
3. without the attribute the DLL is still rejected with the T08-D04a diagnostic;
4. rejections with the cause: a signed DLL (security directory), a DLL that already has an import directory, a
   DLL without TLS, a DLL without zero padding in a read-only section, an unknown RuntimeTls value and RuntimeTls
   without Runtime;
5. precompiled: a producer built with the attribute is consumed without the header and reads 7 (O0 and O2); its
   transported runtime is the adapted copy;
6. artifact cache: a second build hits the cache and the restored consumer reads 7; changing the original makes
   the cache miss and the product carries the copy of the new original.
T08-D04b: RuntimeTls="ImportDirectory" distribuye una copia adaptada de una DLL C con TLS y sin importaciones.
"""
from pathlib import Path
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-tls-copia-adaptada-", dir=r"A:\nsc-interop-build"))
fixtures = Path(__file__).resolve().parent.parent / "tls-dll-distintas"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
crt = "A:\\vc2026-runtime\\lib\\x64\\release\\"
sdk = "A:\\vc2026-runtime\\windows-sdk\\10.0.28000.0\\lib\\"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
DIAGNOSTIC = "has static thread-local storage but no import directory"


def run(command, folder):
    return subprocess.run([str(item) for item in command], cwd=folder, env=env, text=True, encoding="utf-8",
                          errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def require(command, folder, label):
    result = run(command, folder)
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")
    return result.stdout


def build_dll(folder, imported=False, release=False, tls=True):
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    if tls:
        shutil.copyfile(fixtures / "TlsApi2SinImport.c", folder / "api.c")
    else:
        (folder / "api.c").write_text("__declspec(dllexport) int tls_read(void) { return 7; }\n", encoding="utf-8")
    command = [clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"]
    require(command + (["-DTEST_UNUSED_IMPORT"] if imported else []), folder, "clang api.c")
    extra = ["/include:_tls_used", "/export:tls_exported,DATA"] if tls else []
    require([lld, "/nologo", "/dll", "/noentry", "/nodefaultlib", *extra, *(["/release"] if release else []),
             "/out:api.dll", "/implib:api.lib", "api.obj", crt + "libcmt.lib", crt + "libvcruntime.lib",
             sdk + r"ucrt\x64\libucrt.lib", sdk + r"um\x64\kernel32.lib"], folder, "lld-link api.dll")
    return folder / "api.dll"


def pe_layout(data):
    """EN: offsets of the fields T08-D04b may change. ES: desplazamientos de los campos que puede cambiar."""
    nt = struct.unpack_from("<I", data, 0x3C)[0]
    sections, optional_size = struct.unpack_from("<HxxxxxxxxxxxxH", data, nt + 6)
    optional = nt + 24
    directories = optional + 112
    headers = [optional + optional_size + i * 40 for i in range(sections)]
    return optional + 64, directories, headers


def pe_checksum(data, offset):
    total = 0
    for position in range(0, len(data), 2):
        word = data[position] | ((data[position + 1] << 8) if position + 1 < len(data) else 0)
        if offset <= position < offset + 4:
            word = 0
        total += word
        total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (total + len(data)) & 0xFFFFFFFF


def rva_to_offset(data, headers, rva):
    for header in headers:
        size, address, raw_size, raw = struct.unpack_from("<IIII", data, header + 8)
        if address <= rva < address + max(size, raw_size):
            return raw + rva - address
    raise AssertionError(f"RVA {rva:#x} fuera de las secciones")


def check_adapted(original, adapted, label):
    if len(original) != len(adapted):
        raise AssertionError(f"{label}: la copia cambió de tamaño")
    checksum, directories, headers = pe_layout(original)
    allowed = set(range(checksum, checksum + 4)) | set(range(directories + 8, directories + 16))
    for header in headers:
        allowed |= set(range(header + 8, header + 12))
    changed = [i for i in range(len(original)) if original[i] != adapted[i]]
    if not changed or any(i not in allowed for i in changed):
        raise AssertionError(f"{label}: bytes cambiados fuera de los tres campos: {changed[:8]}")
    rva, size = struct.unpack_from("<II", adapted, directories + 8)
    if size != 20 or adapted[rva_to_offset(adapted, headers, rva):][:20] != bytes(20):
        raise AssertionError(f"{label}: el directorio de importación no es un descriptor nulo")
    expected = 0 if struct.unpack_from("<I", original, checksum)[0] == 0 else pe_checksum(adapted, checksum)
    if struct.unpack_from("<I", adapted, checksum)[0] != expected:
        raise AssertionError(f"{label}: CheckSum {struct.unpack_from('<I', adapted, checksum)[0]:#x}, "
                             f"se esperaba {expected:#x}")
    return expected


def reference(prefix, attribute=' RuntimeTls="ImportDirectory"'):
    return (f'<CppLibraryReference Header="ReadApi.h" Language="C" Standard="c11" Library="{prefix}api.lib" '
            f'Runtime="{prefix}api.dll"{attribute} />')


def project(folder, name, source, items, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />{items}</ItemGroup></Project>', encoding="utf-8")


def build(folder, name, kind, output, opt="0", cache=None):
    caching = ["--time", "--artifact-cache=" + str(cache)] if cache else ["--no-artifact-cache"]
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, *caching, "-o", output], folder)


def adapted_copy(folder):
    copies = list((folder / ".nsc" / "runtime-tls").glob("*/api.dll"))
    if len(copies) != 1:
        raise AssertionError(f"se esperaba una copia adaptada en {folder}: {copies}")
    return copies[0]


def run_consumer(exe, runtime):
    if not (exe.parent / "api.dll").exists():
        shutil.copyfile(runtime, exe.parent / "api.dll")
    if (exe.parent / "api.dll").read_bytes() != runtime.read_bytes():
        raise AssertionError(f"{exe.parent}: la api.dll del producto no es la copia adaptada")
    return subprocess.run([str(exe)], cwd=exe.parent).returncode


def check_direct():
    folder = root / "directo"
    original = build_dll(folder / "sin-import")
    before = original.read_bytes()
    (folder / "Main.ns").write_text("int Main() { return tls_read() == 7 ? 0 : 1; }\n", encoding="utf-8")
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    project(folder, "Plain", "Main.ns", reference("sin-import/", ""))
    result = build(folder, "Plain", "exe", "out/Plain.exe")
    if result.returncode == 0 or DIAGNOSTIC not in result.stdout:
        raise AssertionError(f"sin RuntimeTls se esperaba el diagnóstico de T08-D04a:\n{result.stdout}")
    print("sin RuntimeTls: diagnóstico de T08-D04a conservado", flush=True)
    project(folder, "Good", "Main.ns", reference("sin-import/"))
    for opt in ("0", "1", "2", "3"):
        result = build(folder, "Good", "exe", f"out{opt}/Good.exe", opt)
        if result.returncode:
            raise AssertionError(f"directo O{opt}:\n{result.stdout}")
        copy = adapted_copy(folder)
        checksum = check_adapted(before, copy.read_bytes(), f"directo O{opt}")
        code = run_consumer(folder / f"out{opt}" / "Good.exe", copy)
        if code:
            raise AssertionError(f"directo O{opt}: salida {code}")
    if original.read_bytes() != before:
        raise AssertionError("NSC modificó la DLL original")
    print(f"directo O0-O3: TLS inicial 7; original intacto; copia con 3 campos cambiados (CheckSum {checksum:#x})",
          flush=True)
    return folder


def check_release_checksum():
    folder = root / "checksum"
    original = build_dll(folder / "release", release=True)
    data = original.read_bytes()
    checksum_offset = pe_layout(data)[0]
    if struct.unpack_from("<I", data, checksum_offset)[0] == 0:
        raise AssertionError("/release no escribió CheckSum en el original")
    (folder / "Main.ns").write_text("int Main() { return tls_read() == 7 ? 0 : 1; }\n", encoding="utf-8")
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    project(folder, "Sum", "Main.ns", reference("release/"))
    result = build(folder, "Sum", "exe", "out/Sum.exe")
    if result.returncode:
        raise AssertionError(f"original con CheckSum:\n{result.stdout}")
    copy = adapted_copy(folder)
    checksum = check_adapted(data, copy.read_bytes(), "original con CheckSum")
    if run_consumer(folder / "out" / "Sum.exe", copy):
        raise AssertionError("original con CheckSum: ejecución")
    print(f"original con CheckSum: recalculado ({checksum:#x}) y ejecución correcta", flush=True)


def patched(source, folder, mutate):
    folder.mkdir(parents=True)
    for name in ("api.dll", "api.lib", "ReadApi.h"):
        shutil.copyfile(source.parent / name, folder / name)
    data = bytearray((folder / "api.dll").read_bytes())
    mutate(data)
    (folder / "api.dll").write_bytes(bytes(data))


def check_rejections(source):
    folder = root / "rechazos"
    folder.mkdir()
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    (folder / "Main.ns").write_text("int Main() { return tls_read() == 7 ? 0 : 1; }\n", encoding="utf-8")

    def sign(data):
        directories = pe_layout(data)[1]
        struct.pack_into("<II", data, directories + 4 * 8, len(data), 8)

    def fill(data):
        headers = pe_layout(data)[2]
        for header in headers:
            flags = struct.unpack_from("<I", data, header + 36)[0]
            if flags & 0x40 and flags & 0x40000000 and not flags & 0x80000000 and not flags & 0x20000000:
                raw_size = struct.unpack_from("<I", data, header + 16)[0]
                struct.pack_into("<I", data, header + 8, raw_size)

    patched(source, folder / "firmada", sign)
    patched(source, folder / "sin-relleno", fill)
    build_dll(folder / "con-import", imported=True)
    build_dll(folder / "sin-tls", tls=False)
    cases = [
        ("firmada", reference("firmada/"), "is signed (Authenticode)"),
        ("sin-relleno", reference("sin-relleno/"), "has no read-only initialized data section with 20 zero bytes"),
        ("con-import", reference("con-import/"), "already has an import directory"),
        ("sin-tls", reference("sin-tls/"), "has no static TLS directory"),
        ("valor", reference("con-import/", ' RuntimeTls="Copy"'), "RuntimeTls must be ImportDirectory"),
        ("sin-runtime", '<CppLibraryReference Header="ReadApi.h" Language="C" Standard="c11" '
                        'Library="con-import/api.lib" RuntimeTls="ImportDirectory" />',
         "RuntimeTls requires a Runtime"),
    ]
    for name, items, expected in cases:
        before = {p: p.read_bytes() for p in folder.rglob("api.dll")}
        identifier = name.replace("-", "_")
        project(folder, identifier, "Main.ns", items)
        result = build(folder, identifier, "exe", f"out-{name}/M.exe")
        if observe:
            print("rechazo", name, result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2])
            continue
        if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
        if any(p.read_bytes() != data for p, data in before.items()):
            raise AssertionError(f"rechazo {name}: se modificó una DLL")
        print("rechazo", name, "correcto", flush=True)


def check_precompiled():
    folder = root / "precompilado"
    producer = folder / "producer"
    build_dll(producer)
    before = (producer / "api.dll").read_bytes()
    (producer / "P.ns").write_text(
        "namespace TlsRead { public class Q { public static int Read<T>(T unused) { return tls_read(); } } }\n",
        encoding="utf-8")
    project(producer, "TlsRead", "P.ns", reference(""), library=True)
    # EN: publishing next to the original would replace it by the adapted copy: NSC refuses and keeps it.
    # ES: publicar junto al original lo reemplazaria por la copia adaptada: NSC lo rechaza y lo conserva.
    result = build(producer, "TlsRead", "lib", "TlsRead.lib")
    if (result.returncode == 0 or "already contains a different 'api.dll'" not in result.stdout or
            (producer / "api.dll").read_bytes() != before):
        raise AssertionError(f"productor publicado junto al original:\n{result.stdout}")
    print("productor publicado junto a la DLL original: rechazado; original intacto", flush=True)
    result = build(producer, "TlsRead", "lib", "out/TlsRead.lib")
    if result.returncode:
        raise AssertionError(f"productor con RuntimeTls:\n{result.stdout}")
    copy = adapted_copy(producer)
    check_adapted(before, copy.read_bytes(), "productor")
    (producer / "ReadApi.h").unlink()
    (folder / "C.ns").write_text("int Main() { return TlsRead.Q.Read<int>(0) == 7 ? 0 : 1; }\n", encoding="utf-8")
    project(folder, "C", "C.ns", '<LibraryReference Include="producer/out/TlsRead.lib" />')
    for opt in ("0", "2"):
        result = build(folder, "C", "exe", f"out{opt}/C.exe", opt)
        if result.returncode:
            raise AssertionError(f"consumidor precompilado O{opt}:\n{result.stdout}")
        code = run_consumer(folder / f"out{opt}" / "C.exe", copy)
        if code:
            raise AssertionError(f"consumidor precompilado O{opt}: salida {code}")
    if (producer / "api.dll").read_bytes() != before:
        raise AssertionError("productor: NSC modificó la DLL original")
    print("precompilado: el consumidor sin encabezado usa la copia adaptada y lee 7 (O0 y O2)", flush=True)


def check_cache(folder):
    cache = root / "cache"
    for attempt, marker in enumerate(("compilation-bundle-cache: not-found", "compilation-bundle-cache: hit")):
        out = folder / "out-cache"
        if out.exists():
            shutil.rmtree(out)
        result = build(folder, "Good", "exe", "out-cache/Good.exe", "0", cache)
        if result.returncode or marker not in result.stdout:
            raise AssertionError(f"caché intento {attempt}:\n{result.stdout}")
        if run_consumer(out / "Good.exe", adapted_copy(folder)):
            raise AssertionError(f"caché intento {attempt}: ejecución")
    print("caché: acierto en la segunda construcción y ejecución correcta", flush=True)
    # EN: a changed original (here its COFF TimeDateStamp) yields another copy, so the cache must miss.
    # ES: un original cambiado (aqui su TimeDateStamp COFF) da otra copia, asi que la cache debe fallar.
    original = folder / "sin-import" / "api.dll"
    data = bytearray(original.read_bytes())
    stamp = struct.unpack_from("<I", data, 0x3C)[0] + 8
    struct.pack_into("<I", data, stamp, struct.unpack_from("<I", data, stamp)[0] ^ 0x5A5A)
    original.write_bytes(bytes(data))
    out = folder / "out-cache"
    shutil.rmtree(out)
    result = build(folder, "Good", "exe", "out-cache/Good.exe", "0", cache)
    if result.returncode or "compilation-bundle-cache: hit" in result.stdout:
        raise AssertionError(f"caché con el original cambiado:\n{result.stdout}")
    check_adapted(bytes(data), (out / "api.dll").read_bytes(), "caché con el original cambiado")
    if subprocess.run([str(out / "Good.exe")], cwd=out).returncode:
        raise AssertionError("caché con el original cambiado: ejecución")
    print("caché: cambiar el original produce un fallo de caché y la copia nueva", flush=True)


direct = check_direct()
check_release_checksum()
check_rejections(root / "directo" / "sin-import" / "api.dll")
check_precompiled()
check_cache(direct)
print("observación terminada" if observe else "RuntimeTls ImportDirectory verificado:", root)
