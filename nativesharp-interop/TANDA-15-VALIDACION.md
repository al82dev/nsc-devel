# Validación de la tanda 15 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno, un compilador
NSC de desarrollo y el runtime estático de MSVC con la STL de `A:/vc2026-runtime` (toolset 14.52.36815, SDK
10.0.28000.0), que cada guion pasa con `IncludeDirectory` y `--link-lib`. Cada guion crea sus artefactos y
registros en un directorio propio bajo `A:/temp` y no modifica PATH. Los guiones que arrancan por el runtime
de C (`--crt-startup`) enlazan con lld-link (`NSC_LINKER=lld-link.exe`); el de callbacks usa NSLink.

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root` (raíz de
Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-15-excepciones` | excepciones C++ que atraviesan NativeSharp (T15-001 a 007, 013) |
| `pruebas-tanda-15-callbacks` | callbacks NativeSharp llamados por C++ (T15-008 a 012) |
| `pruebas-tanda-15-recursos` | punteros inteligentes, allocators, tipos no copiables y corutinas (T15-014 a 018) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3 e incluye
controles negativos.

`pruebas-tanda-15-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-15-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 15 a 9 sin `--dll` y los controles CTest; sus
resultados y el cierre están en [TANDA-15-VALIDACION-AGRUPADA.md](TANDA-15-VALIDACION-AGRUPADA.md). Los
pendientes de la tanda están en [registro-commits/t15-cierre.md](registro-commits/t15-cierre.md).
