# Validación de la tanda 11 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno y un
compilador NSC de desarrollo. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH. El entorno selecciona NSLink (`NSC_LINKER=nslink.exe`) y la biblioteca
runtime de la construcción (`NSC_WINDOWS_RUNTIME_LIBRARY`).

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root`
(raíz de Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-11-destructores` | destructores `noexcept` implícitos, por defecto y triviales; orden de destrucción (T11-007, T11-013, T11-020) |
| `pruebas-tanda-11-metodos` | métodos de instancia con enums, records, referencias y nombres reservados (T11-005, T11-010) |
| `pruebas-tanda-11-constructores` | constructores, copia, movimiento, eliminados y acceso (T11-006, T11-015, T11-016, T11-017, T11-021) |
| `pruebas-tanda-11-objetos` | layout, alineación, construcción y campos (T11-002, T11-003, T11-008, T11-009, T11-011, T11-012) |
| `pruebas-tanda-11-propiedad` | objetos dinámicos, punteros no propietarios y DLL con CRT (T11-018, T11-019, T11-022) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3 y
exige en el IR de NSC las declaraciones de Clang con sus tipos físicos. Los de constructores, métodos y
destructores incluyen controles negativos. `pruebas-tanda-11-propiedad` con `--dll` enlaza la DLL con
el CRT de `A:/vc2026-runtime`.

`pruebas-tanda-11-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-11-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 11, 10 y 9 sin `--dll` y los controles
CTest; sus resultados y el cierre están en [TANDA-11-VALIDACION-AGRUPADA.md](TANDA-11-VALIDACION-AGRUPADA.md).
Los pendientes de la tanda están en [registro-commits/t11-cierre.md](registro-commits/t11-cierre.md).
