# 221 — Hosts nativos y ABI pública / Native hosts and public ABI

## Resultado / Outcome

**ES.** La matriz de hosts C11 y C++20 cubre ahora la superficie que un SDK nativo necesita para
consumir una DLL NativeSharp sin conocer NSMETA ni el ABI interno del lenguaje: escalares, enums,
structs, fixed arrays, punteros tipados y `void*`, buffers byte/int con longitud, punteros de salida,
callbacks persistidos, handles opacos y ciclo explícito create/get/update/destroy.

**EN.** The C11 and C++20 host matrix now covers the surface a native SDK needs to consume a
NativeSharp DLL without knowing NSMETA or the language-internal ABI: scalars, enums, structs, fixed
arrays, typed and void pointers, byte/int pointer-plus-length buffers, output pointers, retained
callbacks, opaque handles, and explicit create/get/update/destroy lifetime.

## Alcance físico / Physical scope

Los hosts sólo reciben los productos públicos:

```text
Stage48_3HostApi.dll + Stage48_3HostApi.lib + Stage48_3HostApi.h
Stage48_4HostApi.dll + Stage48_4HostApi.lib + Stage48_4HostApi.h
```

No incluyen headers internos de NSC, no enlazan librerías C++ del compilador y no materializan
fuentes NativeSharp. El host C compila como C11 estricto; el host C++ compila como C++20 sin RTTI ni
excepciones y consume el bloque `extern "C"` del mismo header generado.

The hosts include no NSC-private headers, link no compiler C++ libraries, and materialize no
NativeSharp sources. The C host is strict C11; the C++ host is C++20 without RTTI or exceptions and
consumes the generated header's `extern "C"` block.

## Superficie certificada / Certified surface

| Categoría / Category | NativeSharp | C/C++ generado / Generated C/C++ | Evidencia / Evidence |
|---|---|---|---|
| Escalares | `int`, `byte` | `int32_t`, `uint8_t` | suma y checksum UTF-8 |
| Enum | `HostMode` | carrier typedef estable | round trip por valor |
| Struct | `Pair`, `LayoutProbe` | struct + `sizeof/alignof/offsetof` | entrada/retorno por valor |
| Fixed array | `int[4] Values` | `int32_t Values[4]` | round trip y suma 42 |
| Puntero tipado | `int*` | `int32_t*` | identidad y output |
| Puntero crudo | `void*` | `void*` | identidad de dirección |
| Buffer | `int* + count`, `byte* + count` | pointer + `int32_t` | callback/suma/checksum |
| Salida | `int*`, `Pair*` | puntero explícito | escribe 42 y `{21,21}` |
| Callback | `extern C delegate` | function pointer `NS_CALL` | invocación inmediata y retenida |
| Handle | `Counter` | `NS_Counter*` opaco | create/add/get/destroy |

## Cambios concretos / Concrete changes

### `Stage48_3CHostIntegrationTests.cpp`

- Agrega `FixedBlock` con `int[4]` y exige su layout en el header.
- Agrega `NS48_EchoFixed` para probar ABI agregado con fixed array.
- Agrega `NS48_WriteAnswer` y `NS48_WritePair` como patrones de salida explícita.
- Agrega `NS48_Utf8Checksum(byte*, int)` como contrato nativo de texto/buffer sin ownership oculto.
- El host C ejecuta cada caso y la inspección PE exige los 18 imports C exactos.

### `Stage48_4CppHostIntegrationTests.cpp`

- Replica la misma API con prefijo `NS484_`.
- El host C++ usa inicialización agregada, punteros de salida y buffer `uint8_t` desde el header.
- Mantiene el wrapper RAII exclusivamente del lado host sobre el handle opaco; el ABI sigue siendo C.
- La inspección rechaza símbolos decorados o vtables C++ en la frontera.

The C and C++ suites now exercise the same expanded ABI with their respective `NS48_` and `NS484_`
prefixes. C++ convenience remains host-local: the DLL boundary is still a plain C ABI.

## Decisiones / Decisions

1. **`ref/out/in` no se falsifican en C.** La frontera usa punteros explícitos porque ésa es la
   representación nativa interoperable. El header no inventa referencias administradas.
2. **Texto como bytes + longitud.** `string` NativeSharp posee semántica de runtime que no puede
   cruzar una ABI C de forma implícita. UTF-8 se prueba como `byte* + count`.
3. **Clases como handles opacos.** Ningún layout de objeto, descriptor o vtable NativeSharp se
   congela accidentalmente como ABI C.
4. **Callbacks con calling convention explícita.** El typedef generado es la autoridad.
5. **Layout ejecutable.** Los static assertions convierten packing o alineación incorrectos en un
   error del compilador host.

1. **No fake `ref/out/in` in C.** The boundary uses explicit pointers, the interoperable native
   representation, rather than inventing managed references.
2. **Text is bytes plus length.** NativeSharp `string` runtime semantics cannot cross a C ABI
   implicitly; UTF-8 is certified as `byte* + count`.
3. **Classes are opaque handles.** No object layout, descriptor, or NativeSharp vtable becomes an
   accidental C ABI promise.
4. **Callbacks have an explicit calling convention.** The generated typedef is authoritative.
5. **Executable layout contract.** Static assertions turn bad packing/alignment into a host compile
   failure.

## Compiladores host / Host compilers

La matriz automatizada usa Clang en target `x86_64-pc-windows-msvc`, que ejerce exactamente el ABI
MSVC/Win64 y el import library COFF distribuido. El header generado usa únicamente C11/C++20 y
macros MSVC portables (`__declspec`, calling convention vacía en Win64), por lo que `cl.exe` puede
consumir el mismo producto sin una variante del header. La ejecución automática con MSVC queda
condicionada a que ese toolset exista en el entorno de certificación; no se convierte en dependencia
del compilador NSC.

The automated matrix uses Clang targeting `x86_64-pc-windows-msvc`, exercising the exact MSVC/Win64
ABI and distributed COFF import library. The generated header remains valid C11/C++20 for `cl.exe`
without a separate variant; running MSVC itself is conditional on that external toolset being
available and is not an NSC dependency.

## Verificación durante iteración / Iteration verification

Las regresiones se compilan y ejecutan en el baseline acumulativo. En esta entrega no se ejecuta la
matriz de hosts; después del commit sólo se reconstruye desde cero `nsc`, conforme al ciclo acordado.

The regressions compile and execute during the cumulative baseline. This delivery does not run the
host matrix; only a clean `nsc` rebuild follows the commit under the agreed cycle.

## Commit de referencia / Reference commit

**Título ES:** Certifica la ABI pública desde hosts nativos

**Title EN:** Certify the public ABI from native hosts

**Descripción ES:** Amplía los hosts C11 y C++20 con fixed arrays, punteros de salida, buffers UTF-8
y contratos de imports exactos, conservando callbacks y handles opacos sobre una frontera C Win64.

**Description EN:** Expand the C11 and C++20 hosts with fixed arrays, output pointers, UTF-8 buffers,
and exact import contracts while preserving callbacks and opaque handles over a Win64 C boundary.
