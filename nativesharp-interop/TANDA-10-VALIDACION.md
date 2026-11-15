# Validación de la tanda 10 en Windows x64

Las pruebas usan Clang 23.1.0 disponible por nombre, las herramientas LLVM del mismo entorno y un
compilador NSC de desarrollo. Cada guion crea sus artefactos y registros en un directorio propio bajo
`A:/temp` y no modifica PATH. El entorno selecciona NSLink (`NSC_LINKER=nslink.exe`) y la biblioteca
runtime de la construcción (`NSC_WINDOWS_RUNTIME_LIBRARY`). Los ejecutables con thread-locals los
enlaza el propio guion con `lld-link`, porque NSLink no escribe el directorio TLS.

Ejecutar `validar.py` en cada directorio, con `--nsc` (compilador de desarrollo) y `--clang-root`
(raíz de Clang), y otra vez con `--dll` cuando el guion lo admite:

| Fixture | Capacidad |
|---|---|
| `pruebas-tanda-10-enum-cpp-*` | enums C++ por base, scoped, globales y en namespace (T10-002) |
| `pruebas-tanda-10-referencias` | referencias lvalue en funciones libres (T10-003) |
| `pruebas-tanda-10-predeterminados` | argumentos predeterminados evaluados por Clang (T10-010) |
| `pruebas-tanda-10-inline` | definiciones `inline` y `constexpr` emitidas por Clang (T10-012, T10-006) |
| `pruebas-tanda-10-namespaces` | inline namespaces, alias y declaraciones using (T10-002) |
| `pruebas-tanda-10-using-namespace` | directivas using namespace (T10-002) |
| `pruebas-tanda-10-anidados` | records y enums anidados, métodos estáticos (T10-002, T10-005) |
| `pruebas-tanda-10-globales` | globales y campos estáticos de record, enum, arreglo y `thread_local` (T10-004, T10-006, T10-014, T10-015) |
| `pruebas-tanda-10-acceso` | acceso de miembros estáticos (T10-007) |
| `pruebas-tanda-10-estaticos-valor` | records triviales por valor en métodos estáticos (T10-005, T10-013) |
| `pruebas-tanda-10-operadores` | operadores libres (T10-016) |
| `pruebas-tanda-10-integral` | biblioteca integral (T10-001, T10-008, T10-009, T10-011, T10-017) |

Salvo los `enum-cpp-*`, anteriores al plan de la tanda, cada fixture compara un control C++ compilado
por Clang con el consumidor NativeSharp de O0 a O3, exige en el IR de NSC las declaraciones de Clang con
sus tipos físicos, consume una plantilla precompilada con y sin encabezado e incluye controles
negativos.

`pruebas-tanda-10-auditoria/sonda.py` es la sonda de la [auditoría inicial](TANDA-10-AUDITORIA-INICIAL.md);
no es una prueba de soporte.

La regresión agrupada ejecuta todos los guiones de las tandas 10 y 9 y los controles CTest; sus
resultados y el cierre están en [TANDA-10-VALIDACION-AGRUPADA.md](TANDA-10-VALIDACION-AGRUPADA.md). Los
pendientes de la tanda están en [registro-commits/t10-cierre.md](registro-commits/t10-cierre.md).
