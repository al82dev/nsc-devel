# Tanda 15 — Runtime, linker e interop

## Alcance e inventario

La matriz canónica contiene 70 IDs de `runtime`, `linker` e `interop`.

| Clase | IDs |
|---|---:|
| C++ O0 consolidado | 27 |
| Casos directos O0 externalizados | 3 |
| C++ con rutas O1/O2/O3 | 39 |
| Verificador CMake especializado para tanda 16 | 1 |
| Total | 70 |

Los 39 contratos no-O0 no se ejecutaron ni se simplificaron. El verificador especializado mantiene
su propietario histórico y queda enrutado al adaptador de suites.

## Implementación

- `RuntimeLinkerInteropUnitTests.cpp` integra 27 propietarios históricos.
- `RuntimeLinkerInteropLegacyEntries.def` fija membresía, componente e identidad.
- `runtime_linker_interop_adapters/*.cpp` preserva las aserciones originales.
- `tests/cases/runtime-linker` externaliza el caso negativo de símbolo runtime reservado y las
  consultas públicas `--print-linker` y `--dump-runtime-llvm`.
- El target consolidado recibe el mismo `NSC_TOOLCHAIN_CONTRACT_PATH` que el propietario histórico
  Stage62; el archivo sigue procediendo del árbol de compilación seleccionado.

## Validación

| Grupo | Resultado |
|---|---:|
| Interop C++ O0 | 5/5 |
| Linker C++ O0 | 9/9 |
| Runtime C++ O0 | 13/13 |
| Casos directos runtime/linker | 3/3 |
| Total ejecutado | 30/30 |

Las ejecuciones fijaron explícitamente el NSC construido, NSLink de A:, el runtime generado de A:
y el bibliotecario disponible. No se instaló, desplegó ni copió nada desde o hacia C:.

## Incidencia de construcción

Stage62 no compiló inicialmente porque su macro `NSC_TOOLCHAIN_CONTRACT_PATH` era privada del target
histórico. Se trasladó la misma definición al target consolidado, apuntando a
`${NSC_TEST_COMPILER_BUILD_DIR}/generated/NativeSharpToolchain.contract`. La reconstrucción posterior
fue correcta. No se cambió la prueba ni el contrato generado.

## Complejidad y propiedad

El registro es iterativo O(n) y O(1) adicional. Cada propietario conserva sus recursos, limpieza y
aserciones. Los casos externos usan workspaces aislados y limpieza iterativa bajo A:.

## Exclusiones explícitas

No se ejecutaron matrices O2, pruebas cruzadas de linkers, LTO, host C/C++ optimizado ni contratos
runtime O2. Su estado es `BLOCKED_BY_SCOPE_O0_ONLY`, no aprobación ni descarte.
