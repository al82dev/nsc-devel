# Validación de la tanda 16 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno, un compilador
NSC de desarrollo y el runtime estático de MSVC con la STL de `A:/vc2026-runtime` (toolset 14.52.36815, SDK
10.0.28000.0), que cada guion pasa con `IncludeDirectory` y `--link-lib`. Cada guion crea sus artefactos y
registros en un directorio propio bajo `A:/temp` y no modifica PATH. Los guiones que arrancan por el runtime
de C (`--crt-startup`) enlazan con lld-link; el de enlace prueba NSLink y lld-link.

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root` (raíz de
Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-16-arranque` | entrada, constructores y destructores globales, orden, DLL y `DllMain` (T16-001 a 005, 008, 012, 013) |
| `pruebas-tanda-16-tls` | `thread_local` con inicialización dinámica desde dos hilos (T16-006, 007) |
| `pruebas-tanda-16-enlace` | enlace separado, símbolos duplicados y ThinLTO (T16-014 a 016) |
| `pruebas-tanda-16-atomicos` | atómicos compartidos entre callbacks NativeSharp e hilos C++ (T16-017) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3 e incluye
controles negativos. Las políticas T16-009 a 011 están en
[registro-commits/t16-politicas.md](registro-commits/t16-politicas.md).

`pruebas-tanda-16-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-16-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 16 a 9 sin `--dll` y los controles CTest; sus
resultados y el cierre están en [TANDA-16-VALIDACION-AGRUPADA.md](TANDA-16-VALIDACION-AGRUPADA.md). Los
pendientes de la tanda están en [registro-commits/t16-cierre.md](registro-commits/t16-cierre.md).
