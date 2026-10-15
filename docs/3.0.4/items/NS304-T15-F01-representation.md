# NS304-T15-F01 — Representación decimal

- **Estado**: CERRADO (contrato ejecutable de representación + layout guard verificados)
- **Rama**: development

## SCOPE LOCK

- **Objetivo**: congelar una autoridad ejecutable para la representación de `decimal`: coeficiente
  sin signo de 96 bits, escala de 0 a 28, signo independiente y payload total de 128 bits.
- **Naturaleza**: contrato interno del compilador y pruebas unitarias de representación; no cambia
  sintaxis, superficie pública NativeSharp, ABI ya observable ni semántica aritmética.
- **Revisión inicial**: `06c3a700af62fa7073c87b6b90ce24378e38c22c`.
- **Archivos permitidos**: `NumericSemantics.h/.cpp`, su prueba unitaria existente, este expediente,
  el tracking maestro y `A:\restantes-deepseek.md`.
- **Archivos protegidos**: todo T13-F07 y sus productores; parser/literal decimal de T15-F02;
  lowering aritmético de T15-F04; conversiones de T15-F06; formatting de T15-F08; CMake, Builder,
  manifiestos y configuración.
- **Invariantes**: 16 bytes; coeficiente máximo `2^96-1`; escala máxima 28; cero sin signo en la
  forma normalizada; representaciones equivalentes normalizan al mismo valor sin destruir la escala
  almacenada del operando original.
- **Pruebas autorizadas**: únicamente la prueba unitaria numérica focal y `TL-LANG-420` como guard,
  siempre que puedan ejecutarse íntegramente desde `A:`.
- **Acciones prohibidas**: batería completa, baseline, deploy, push/fetch/pull, cambio de toolchain,
  acceso fuera de `A:` salvo la toolchain oficial `C:\clang-llvm`, versionamiento NSMETA y cualquier
  modificación del trabajo de DeepSeek.
- **Criterio de cierre**: contrato de representación compilado y probado, layout guard preservado,
  documentación y tracking coherentes, y commit local aislado. Fuentes, temporales, builds, logs y
  artefactos permanecen en `A:`.

## Resultado (tipo + layout — 2026-10-14)

- **Builtin**: `decimal` se resuelve como tipo valor (`builtin:decimal`) en `ProjectNameLookup` y
  `ProjectMemberModel::canonicalTypeKey`; el parser ya no lo difiere con NS3049.
- **Layout**: `TargetDataLayout::BuiltinLayoutKind::Decimal` = 16 bytes / alineación 8; LLVM lo
  representa como `i128`.
- Verificado: `struct S { decimal d; }` + `(int)sizeof(S)` → **16**. Registrado `TL-LANG-420` (PASS).

## Contrato de representación cerrado — 2026-10-14

- `Decimal128Bits` fija dos palabras little-endian de 64 bits: coeficiente sin signo de 96 bits,
  escala de 6 bits limitada a 0–28, bits reservados en cero y signo independiente.
- `makeDecimal128Bits` no construye escalas fuera de rango; `decimal128BitsAreValid` rechaza estados
  reservados. No existen patrones para NaN o infinito.
- La normalización elimina ceros decimales finales mientras conserva el operando almacenado; todo
  cero normalizado queda positivo y con escala cero.
- Equality y hashing operan sobre la forma normalizada, de modo que `1.0` y `1.00` son equivalentes
  y producen el mismo hash estable.
- `static_assert` congela tamaño 16 y alineación 8; la prueba cubre coeficiente máximo, escala máxima,
  signo, escala inválida, bits reservados, equivalencia entre escalas y cero negativo escalado.

## Validación focal — 2026-10-14

- `A:\nsc-devel\build-lsp-debug\bin\nsc_ns3_t14_numeric_tests.exe`: **PASS 10/10**, salida 0,
  Debug/O0 (`/Od`) con `C:\clang-llvm\bin\clang-cl.exe`; SHA-256
  `fe2870c1c834822c1f76e518886866fb30f7d95cdd9f6c9bc8783754ab0d30f7`.
- `TL-LANG-420`, ejecutado como caso único: **PASS**, salida 0. Compilador probado:
  `A:\nsc-devel\build-clang\bin\nsc.exe`, SHA-256
  `17c931fe802aeee61e853faeb0db8323328aae99d498eccf0715aa5bdd1312a6`; invocación del fixture
  con `-O0 -g`; `sizeof(decimal) == 16`.

## Fuera de alcance de F01

Lexer/literales y round-trip pertenecen a F02; ABI/mangling/FFI a F03; aritmética y redondeo a F04;
conversiones a F06; lowering consteval/runtime a F07; formatting/parsing y traits a F08. Este cierre
no crea ni reactiva versionamiento NSMETA.
