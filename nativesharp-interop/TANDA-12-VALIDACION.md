# Validación de la tanda 12 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno y un
compilador NSC de desarrollo. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH. El entorno selecciona NSLink (`NSC_LINKER=nslink.exe`) y la biblioteca
runtime de la construcción (`NSC_WINDOWS_RUNTIME_LIBRARY`).

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root`
(raíz de Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-12-herencia` | bases, métodos heredados, despacho virtual, herencia múltiple y virtual, conversiones, destrucción por la base, RTTI y `dynamic_cast` (T12-001 a T12-012) |
| `pruebas-tanda-12-valores` | records trivialmente copiables con bases como valores y su ABI de retorno (T12-001, T12-003) |
| `pruebas-tanda-12-operadores-miembro` | operadores miembro como operadores NativeSharp (T12-004) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3 y
exige que cada símbolo C++ que NSC llama directamente tenga los tipos físicos de Clang; los de herencia
y operadores cuentan además las llamadas por adaptador. Todos incluyen controles negativos salvo
`pruebas-tanda-12-valores`. `pruebas-tanda-12-herencia` con `--dll` enlaza la DLL con el CRT de
`A:/vc2026-runtime`; sin `--dll` la biblioteca se compila con `-fno-rtti`.

`pruebas-tanda-12-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-12-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 12, 11, 10 y 9 sin `--dll` y los controles
CTest; sus resultados y el cierre están en [TANDA-12-VALIDACION-AGRUPADA.md](TANDA-12-VALIDACION-AGRUPADA.md).
Los pendientes de la tanda están en [registro-commits/t12-cierre.md](registro-commits/t12-cierre.md).
