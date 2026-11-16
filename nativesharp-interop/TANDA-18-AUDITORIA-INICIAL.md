# Tanda 18: auditoría inicial

Base: `2dcb699d5` (cierre de la tanda 17). Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`.

Los experimentos están en `build-claude-t08/tools/exp-t18-*.py`; cada uno deja su evidencia en `A:/temp`.

## Bibliotecas reales disponibles

Sin red hacia los repositorios de código, las bibliotecas salen de las fuentes que ya están en el equipo
(`A:/al82dev-clang-deps`): zlib 1.3.2 (C), ICU4C 80 (C y C++), zstd, libxml2, libiconv, libffi, curl, z3 y
cpp-httplib. La STL de MSVC del toolset 14.52.36815 es la biblioteca de plantillas en encabezados.

- **zlib 1.3.2 (T18-008, T18-009).** Once unidades compiladas con Clang (`-std=c17 -O2`). Desde NativeSharp
  funcionan `compress`, `uncompress`, `crc32`, `adler32`, `compressBound` y `zlibVersion` (`sbyte const*`) y la
  API de flujo con un `z_stream` local (`deflateInit_`, `deflate`, `inflate`…), con el asignador por defecto,
  que usa el CRT (`--crt-startup`). Experimento: `A:/temp/nsc-t18-zlib-r3xua6v7`. Límites observados:
  asignar `null` a un campo puntero a función (`zalloc = Z_NULL`) se rechaza; `sizeof` es `nuint` y
  `deflateInit_` pide `int`; un struct C no se crea con `new`.
- **ICU4C 80, `icuuc` (T18-010, T18-011).** Las 203 unidades de `common` y `stubdata` compilan con Clang
  (`-std=c++17 -O2`, 151 s en serie). Clases con vida no trivial y destructor virtual: `ParsePosition`,
  `Edits` (memoria propia que crece) y `ErrorCode`. Ninguna se puede usar como local: `C++ local lifetime
  currently requires a non-throwing constructor and destructor`. Sus constructores no son `noexcept`, como
  casi todos los de una biblioteca C++ real. Causa en el código: `ProjectCompilation` exige
  `cppLifetimeNonThrowing` en el constructor de un local C++ (`cppNoexceptLocalLifetimeOnly_`), aunque el
  local se construye en su lugar y su indicador de destrucción se activa solo cuando el constructor vuelve
  (`lowerLocalDeclarationStatement`), así que una excepción ya lo deja sin destruir. Además:
  `icu::UnicodeString` no importa sus constructores (`struct has only an implicit parameterless
  constructor`), el alias `icu` no sirve como prefijo de tipo (`expected ';'`; `icu_80` sí) y `UBool` es
  entero, no `bool`. Experimentos: `A:/temp/nsc-t18-icu-ns-znbbzl05`, `A:/temp/nsc-t18-icu-ns-g8vmgec5`.
- **STL (T18-012, T18-013).** Funcionan `std::array<int, 4>` (`_Elems`, `size`, `fill` con `in`, local sin
  inicializador), `std::numeric_limits<sbyte>`/`<short>`, `std::integral_constant<int, 5>`, `std::gcd`,
  `std::lcm`, `std::popcount`, `std::bit_width`, `std::countl_zero`, `std::has_single_bit` y `std::rotl`.
  No: `std::min`, `std::max` y `std::clamp` (devuelven `const T&`), `std::pair` (constructor plantilla),
  `std::optional<int>` y `std::string_view` como locales, `std::midpoint` (la sobrecarga de punteros no se
  importa) y `std::array::at` (sobrecargas con referencia). Experimentos: `A:/temp/nsc-t18-stl-2du4rl0y`,
  `A:/temp/nsc-t18-stl-zepvi9fq`.

## Distribución (T18-017)

`llvm-readobj --coff-imports` de `nsc.exe`: solo DLL del sistema (`KERNEL32`, `ADVAPI32`, `ntdll`,
`VERSION`, `OLEAUT32`, y diferidas `ole32` y `SHELL32`); LLVM, Clang y el CRT van enlazados estáticamente.
Para compilar un programa con bibliotecas C/C++, NSC necesita además la raíz de Clang (`--cpp-clang-root`,
por sus encabezados internos y `clang.exe` como referencia del controlador), el runtime NativeSharp
(`NativeSharp.Runtime.Win32.lib`), un enlazador (`nslink.exe` o `lld-link.exe`) y, para bibliotecas que usan
el runtime de C, los encabezados y bibliotecas de MSVC y del SDK.

## Plan

Ver [SCOPE-LOCK-TANDA-18.md](SCOPE-LOCK-TANDA-18.md).
