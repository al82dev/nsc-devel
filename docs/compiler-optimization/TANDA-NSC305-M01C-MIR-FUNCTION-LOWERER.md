# Tanda NSC 3.0.5 M01C — extracción de `MirFunctionLowerer`

## Clasificación documental

- **IMPLEMENTACIÓN VIGENTE:** extracción interna descrita en este expediente.
- **HECHO COMPROBADO:** observación directa del árbol, del diferencial o de una comprobación estática.
- **NO VERIFICADO:** comportamiento que requeriría compilación o ejecución y queda diferido.

## Propósito

M01C separa del coordinador MIR el tramo que transforma funciones HIR en funciones MIR, conserva el
despacho paralelo existente, publica los resultados en orden determinista y propaga los layouts de frames
async y generator. Es un refactor interno por movimiento puro: no modifica los algoritmos, el scheduling,
los diagnósticos, el orden de publicación ni las estructuras producidas.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Repositorio y rama | `A:\nsc-devel`, rama `development`. |
| Naturaleza | Refactor interno no funcional por extracción física. |
| Objetivo exacto | Mover el tramo completo de lowering de funciones a `Frontend/MIR/MirFunctionLowerer.cpp`. |
| Revisión inicial | `fa8dabd547c757e10c6a2f48aed31084c429f93b`. |
| Oráculo | Comportamiento congelado de NSC 3.0.4 y estado inmediatamente anterior a M01C. |
| Contratos preservados | Sintaxis, semántica, API, ABI, NSMETA, diagnósticos, orden, ownership y formatos. |
| Contratos preservados | También artefactos, determinismo, paralelismo, CLI y perfiles O0/O1/O2/O3. |
| Archivos de producto autorizados | Header privado MIR, coordinador, lowerer y las dos listas de sources MIR. |
| Archivo documental autorizado | Este expediente. |
| Archivos protegidos | Pruebas, fixtures, consumidores, runtime, linker, LLVM y metadata pública. |
| Trabajo concurrente protegido | HIR, parser y pruebas ajenas visibles en el worktree compartido. |
| Validación autorizada ahora | Gates estáticos de identidad, referencias, diferencial, whitespace y stage. |
| Validación prohibida ahora | Compilación, pruebas, baseline, suite integral, deploy e instalación. |
| Red y unidades | Sin `push`, `pull` ni `fetch`; todas las operaciones controladas permanecen en `A:`. |
| Herramientas | Sólo herramientas del PATH nativo de Windows; sin MSYS, worktrees ni workspaces. |
| Criterio de terminación | Extracción completa, helpers dentro del límite, infraestructura y diferencial aislado. |

## Presupuesto y frontera causal

La entrada es el punto del `build` situado después de construir las tablas de despacho y sus planes de
destrucción. La operación extraída termina antes del lowering de inicializadores de campos. Sus entradas son
los modelos ya construidos, el número de workers y el canal de error. Sus salidas son:

- `functions_`;
- `functionByMember_`;
- `functionByNamespaceFunction_`;
- cuerpos MIR producidos por `lowerNode`;
- ABI físico por callable;
- layouts de frames async, generator y async-generator;
- contrato de cleanup y exception flow por función;
- el mismo primer diagnóstico en `error` y el mismo retorno fallido.

No forman parte de M01C:

- cambiar la máquina de nodos MIR;
- convertir las lambdas recursivas existentes a worklists;
- alterar la detección de liveness o el layout de frames;
- cambiar el pool de workers o su granularidad;
- optimizar búsquedas, complejidad o allocations;
- modificar caché, metadata, ABI o contratos públicos.

## Diseño mecánico

`ProjectOopMirModel` recibe los métodos privados `lowerFunctions`, `lowerOneFunction` y
`configureFunctionLowerer`. `build` invoca `lowerFunctions` exactamente donde residía el bloque original y
propaga `false` sin traducir el diagnóstico. El nuevo translation unit contiene:

1. la recolección y validación del contrato de cleanup y exception flow;
2. la copia de metadata, configuración ABI y lowering de una función;
3. los recorridos preexistentes para liveness y layouts async/generator;
4. la configuración de cada lowerer por worker;
5. el merge indexado en el orden de `hirModel.functions()`;
6. la propagación de frames hacia `$drop`, constructores generator, `$moveNext$poll` y `$poll`.

La primera extracción conservó textualmente los dos bloques de origen. Después se dividieron
mecánicamente en helpers cohesionados porque el lineamiento vigente prohíbe funciones de más de 60 líneas
ejecutables. Esa segunda operación no cambia comparaciones, recorridos, errores ni orden de efectos.

La interfaz nueva es privada. No se agrega tipo público, símbolo ABI, opción CLI ni formato persistente.

## Contratos e invariantes preservados

1. Cada callable copia los mismos campos HIR al mismo `ProjectOopMirFunction`.
2. El cálculo de ABI conserva return passing, parámetros ocultos y ordinales físicos.
3. Los imports C/native continúan sin cuerpo y siguen fluyendo al backend como declaraciones.
4. El lowering serial y paralelo usa la misma selección por `workerCount`.
5. Cada worker recibe los mismos modelos y el mismo catálogo de layouts estructurales.
6. Los resultados se publican por índice HIR, no por orden de terminación de workers.
7. El primer error conserva texto, contexto de función y orden determinista.
8. Cleanup regions y exception flow conservan orden, deduplicación y validación de referencias.
9. La liveness async/generator conserva los mismos recorridos, comparaciones y offsets.
10. La inserción de `GeneratorFrameInit` conserva posición, tipo y local ordinal.
11. El bloque de inicializadores de campos permanece después del lowering de funciones.
12. M01C no cambia schema MIR, API, ABI, NSMETA, diagnósticos ni artefactos intencionalmente.

## Evidencia mecánica de movimiento y descomposición

La comparación se realizó contra la revisión inicial, tomando los bloques por su primer símbolo y número de
líneas, y comparándolos con ordinal y mayúsculas significativos.

| Bloque inicial | Origen | Primer destino | Líneas | Identidad | SHA-256 normalizado |
|---|---:|---:|---:|---|---|
| `finalizeCleanupContract` | 954 | 15 | 131 | Exacta | `8AF9D3D6...0EA4A85` |
| Lowering/publicación | 2150 | 159 | 553 | Exacta | `6AB72F16...95205D5` |

Hashes completos:

- `finalizeCleanupContract`: `8AF9D3D629787ADF44DEAF9750E6146DBA1AAC7248EAFE32921E0E81C0EA4A85`;
- lowering/publicación: `6AB72F161094ED7060A8D5B1BCD21EB93187A8B191A977EB7383D3F5295205D5`.

- Líneas del coordinador antes de M01C: 4,855.
- Líneas del coordinador después de M01C: 4,183.
- Líneas del nuevo lowerer final: 788.
- Los 684 renglones productivos fueron primero trasladados con identidad textual exacta.
- El estado final los distribuye en 30 funciones: ninguna supera 60 líneas ejecutables.
- Máximo medido: 47 líneas ejecutables en `copyFunctionMetadata`.
- `lowerFunctions`: 40 líneas ejecutables.
- `lowerOneFunction`: 16 líneas ejecutables.
- `finalizeCleanupContract`: 10 líneas ejecutables.

Los hashes prueban la identidad de la etapa inicial de movimiento, no identidad textual del archivo final.
La paridad del estado final se comprobó además mediante inventarios estáticos:

- literales ejecutables: 15 en origen y 15 en destino, sin faltantes ni extras;
- campos de función normalizados: 54 en origen y 54 en destino, sin faltantes ni extras;
- clases `ProjectOopMirNodeKind`: 24 en origen y 24 en destino, sin faltantes ni extras;
- clases de cleanup: 5 en origen y 5 en destino;
- clases de salida no local: 7 en origen y 7 en destino;
- clases de ABI passing: 2 en origen y 2 en destino.

La revisión manual confirmó que el lowering usa el lowerer serial o por worker para `lowerNode`, pero conserva
del coordinador el catálogo `structLayouts_` y `panicProfile_`, igual que las capturas del lambda original.
No sustituye la compilación ni las pruebas diferidas.

## Complejidad, recursión y rendimiento

M01C no cambia complejidad temporal, espacial, allocations, locality ni scheduling. El paralelismo por
callable permanece idéntico. Las lambdas recursivas preexistentes se expresan ahora como helpers recursivos
nombrados, pero conservan el mismo recorrido preorden, la misma secuencia de hijos y los mismos puntos de
retorno. M01C no afirma haber eliminado recursión. La migración iterativa corresponde a M02 y no se oculta
dentro de esta extracción.

No se reclama mejora de tiempo de compilación del producto. El beneficio de esta hoja es una frontera física
cohesionada que reduce el translation unit coordinador y permite trabajar el lowering sin mezclar layouts,
campos estáticos, caché o la máquina de nodos.

## Gates estáticos

- conservar los hashes de la extracción textual inicial contra la revisión base;
- comprobar paridad de literales, campos, clases de nodo, cleanup, salidas y ABI;
- comprobar una definición y una llamada privadas de `lowerFunctions`;
- comprobar firmas y llamadas de `lowerOneFunction` con layouts y panic profile del coordinador;
- comprobar una referencia al nuevo translation unit en cada lista CMake autorizada;
- comprobar cero funciones nuevas o modificadas por encima de 60 líneas ejecutables;
- comprobar cero copias residuales de los bloques en el coordinador;
- comprobar el límite de 120 caracteres en líneas nuevas;
- ejecutar `git diff --check`;
- revisar el diferencial completo y mantener el índice vacío;
- excluir HIR, parser y pruebas ajenas de cualquier integración.

## Integración de build y Builder 3

- `src/Frontend/MIR/MirFunctionLowerer.cpp` está registrado una vez en `nsc_frontend` del CMake raíz.
- La misma fuente está registrada una vez en `nsc_component_tests` de
  `tests/compiler-suite/CMakeLists.txt`.
- NSC Builder 3 configura el CMake raíz; no mantiene un catálogo paralelo de fuentes de frontend.
- El validador vigente `validate_source_registration.py` compara el producto con la suite durante la
  configuración de pruebas. Su contrato fue inspeccionado, pero no se ejecutó por la prohibición vigente de
  ejecutar pruebas o scripts de validación.
- M01C no agrega pruebas, fixtures ni targets, por lo que no existe otra alta de infraestructura aplicable.

## Validación focal ejecutada

La validación focal se ejecutó el 18 de octubre de 2026 con `TEMP` y `TMP` en `A:\temp`, sin
baseline ni suite completa:

| Gate | Resultado | Duración | Evidencia |
|---|---:|---:|---|
| Build productivo `nsc` | PASS, exit 0 | 15.5 s | `A:\temp\m01cd-build-nsc.log` |
| Build de seis targets MIR | PASS, exit 0 | 46.7 s | `A:\temp\m01cd-build-unit-targets.log` |
| Cinco CTest MIR dedicados | PASS 5/5 | 0.64 s acumulados | `A:\temp\m01cd-ctest-mir-focused.log` |
| Build `nsc_component_tests` | PASS, exit 0 | 14.1 s | `A:\temp\m01cd-build-component-runner.log` |
| Project OOP completo | PASS 157/157, exit 0 | 5.951 s | `A:\temp\m01cd-project-oop-lowering-with-nslink.log` |
| TL-LANG-377, 378 y 438 O0 | PASS 3/3 | 0.785/0.566/0.551 s | `A:\temp\m01cd-tl-lang-*.log` |

Los cinco CTest dedicados fueron `nsc.stage45.mir-exceptional-cfg` y los casos MIR Item395 a
Item398. Los tres casos compiler-suite construyen y ejecutan futuros O0, incluidas rutas async,
future local e intermodular.

El ejecutable completo `nsc.project-oop-lowering` se repitió una sola vez después de corregir el
fixture H02S13B, con `NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe`. Ese NSLink ya existía;
no se construyó ni modificó. Las 157 comprobaciones pasaron y el proceso terminó con código cero.

No se ejecutaron baseline, batería completa, deploy, instalación ni medición de rendimiento.

## Estado final

- Auditoría estática del movimiento y descomposición: aprobada.
- Funciones nuevas/modificadas por encima de 60 ejecutables: cero.
- Auditoría manual de referencias de build: aprobada; una referencia en producto y una en suite.
- Índice de Git: vacío; no se prepararon archivos concurrentes.
- Compilación: PASS en producto, seis targets MIR y runner compiler-suite.
- Pruebas focales M01: cinco CTest MIR dedicados y tres casos O0 PASS.
- Prueba amplia `nsc.project-oop-lowering`: PASS 157/157, exit 0.
- Equivalencia de artefactos: no ejecutada.
- Rendimiento: no medido.

`CIERRE ESTÁTICO COMPLETO; BUILD Y COBERTURA FOCAL M01 APROBADOS`.
