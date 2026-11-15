# Validación de la tanda 13 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno y un
compilador NSC de desarrollo. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH. El entorno selecciona NSLink (`NSC_LINKER=nslink.exe`) y la biblioteca
runtime de la construcción (`NSC_WINDOWS_RUNTIME_LIBRARY`).

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root`
(raíz de Clang), y otra vez con `--dll`:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-13-especializaciones` | especializaciones de clase nombradas en el encabezado (T13-003, 004, 010, 011, 012, 015, 018, 023) |
| `pruebas-tanda-13-plantillas-ns` | especializaciones de clase y de función escritas en NativeSharp, parámetros no tipo, plantilla de plantilla, paquetes, concepts y rechazos de Clang (T13-002, 005, 006, 010, 012, 013, 014, 016, 017) |
| `pruebas-tanda-13-deduccion` | llamadas a plantillas de función con argumentos deducidos (T13-005, 014) |
| `pruebas-tanda-13-constexpr` | constantes y resultados `constexpr` evaluados por Clang (T13-021, 022) |
| `pruebas-tanda-13-structs-ns` | structs NativeSharp como argumentos (T13-007, 008, 009, 024, 025) |
| `pruebas-tanda-13-comdat` | COMDAT y la misma especialización desde dos módulos, con NSLink y lld-link (T13-019, 020) |

Cada fixture compara un control C++ compilado por Clang con el consumidor NativeSharp de O0 a O3 y exige
que cada símbolo C++ que NSC llama tenga los tipos físicos de Clang (el de COMDAT comprueba además las
secciones COMDAT con `llvm-readobj`). Todos incluyen controles negativos con su contraparte salvo
`pruebas-tanda-13-comdat`.

`pruebas-tanda-13-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-13-AUDITORIA-INICIAL.md);
no es una prueba de soporte. Al cierre pasa sus 12 casos.

La regresión agrupada ejecuta todos los guiones de las tandas 13, 12, 11, 10 y 9 sin `--dll` y los
controles CTest; sus resultados y el cierre están en
[TANDA-13-VALIDACION-AGRUPADA.md](TANDA-13-VALIDACION-AGRUPADA.md). Los pendientes de la tanda están en
[registro-commits/t13-cierre.md](registro-commits/t13-cierre.md).
