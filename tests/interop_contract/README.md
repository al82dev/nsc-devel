# `tests/interop_contract/` — la matriz contractual de interop

Esta carpeta existe por la sección 5 del roadmap del compilador
(`01_Compiler_Roadmap.md`): los casos contractuales de interop tienen que vivir como **proyectos
NativeSharp reales** —abribles, compilables a mano, diffeables— y no solo como literales de string
dentro de un `.cpp`. Cada fixture es un `.nsproj` + `Main.ns` autocontenido.

Ninguno de estos archivos es decoración: las suites
`tests/unit/Stage56C1InteropCompletionTests.cpp` y
`tests/unit/Stage56C2C4LanguageCompletionTests.cpp` compilan cada fixture desde su propio
directorio (vía `NSC_SOURCE_DIR`), así que si uno se pudre, `ctest` lo detecta.

## Estado de la matriz de 15 casos — COMPLETA

| # | Caso | Estado |
|---|---|---|
| 1 | DLL import | cubierto por `Stage55XInteropFoundationTests` (ejecución real contra user32/kernel32) |
| 2 | static `.lib` | cubierto por `Stage55XInteropFoundationTests` (`--link-lib` contra un archive de clang) |
| 3 | function pointer parameter | **`c1_03_function_pointer_parameter/`** |
| 4 | function pointer return | **`c1_04_function_pointer_return/`** |
| 5 | raw pointer cast | **`c1_05_raw_pointer_cast/`** |
| 6 | callback | cubierto por `Stage55XInteropFoundationTests` (host C llamando código NativeSharp por puntero) |
| 7 | callback with aggregate | cubierto por `Stage55XInteropFoundationTests` (M4 de 64 bytes entrando a código NS) |
| 8 | direct aggregate | Stage 40.2 / `Stage48*HostIntegrationTests`, + matriz de tamaños contra clang (C5) |
| 9 | indirect aggregate | Stage 40.2 / `Stage48*HostIntegrationTests`, + matriz de tamaños contra clang (C5) |
| 10 | sret | Stage 40.2 / `Stage48*HostIntegrationTests` |
| 11 | fixed field array | **`c2_11_fixed_field_array/`** |
| 12 | union | **`c2_12_union/`** |
| 13 | two libraries with same symbol | **`c1_13_two_libraries_same_symbol/`** |
| 14 | callback exception boundary | **`c1_14_callback_exception_boundary/`** |
| 15 | UTF-16 helper ABI | cubierto por `Stage55XInteropFoundationTests` (convención `ushort*` = LPCWSTR) |

Fuera de la matriz de 15, pero de la misma naturaleza:

| Caso | Fixture |
|---|---|
| `ref` / `out` / `in` (fase C4) | **`c4_ref_out_in/`** |

## Certificación de layout contra un toolchain foráneo

Las filas 8, 9, 11 y 12 no se verifican solo contra la opinión de este compilador. La suite de
C2–C4 genera el header C del proyecto —que declara cada agregado junto con `NS_STATIC_ASSERT` de
tamaño, alineación y offset de cada campo— y le pide a **clang, apuntando al ABI de MSVC, con
`-Werror`**, que lo compile. Si NativeSharp y clang discreparan sobre un array embebido, una
`union`, un `pack(N)` o un `align(N)`, eso es un build roto, no un test verde con un número
equivocado adentro. La misma suite corre la matriz de tamaños del roadmap (sección 4):
**1, 2, 3, 4, 8, 12, 16, 24, 64, 80 bytes**.

Esto no necesita Wine y por lo tanto es un chequeo DURO en cualquier máquina con clang.

## Compilar un fixture a mano

```
nsc --emit-project-oop-exe tests/interop_contract/c2_12_union/c2_12_union.nsproj -O0 -o case12.exe
case12.exe
echo %ERRORLEVEL%
```

Códigos de salida esperados:

- `c1_03`, `c1_04`, `c1_05`, `c2_11`, `c2_12`, `c4_ref_out_in` → **42**.
- `c1_13` → **compila y linkea**; no está pensado para ejecutarse (`a.dll`/`b.dll` no existen a
  propósito: el caso es que dos identidades que comparten nombre de símbolo puedan coexistir).
- `c1_14` → **141** (`128 + LlvmTrapReason::CallbackExceptionEscape`), no 0. Una salida 0 ahí
  significaría que la excepción se escapó por el borde C.
