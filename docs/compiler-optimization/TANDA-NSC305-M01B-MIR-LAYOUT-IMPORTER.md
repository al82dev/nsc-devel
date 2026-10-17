# Tanda NSC 3.0.5 M01B — extracción de `MirLayoutImporter`

## Clasificación documental

- **IMPLEMENTACIÓN VIGENTE:** extracción interna descrita en este expediente.
- **HECHO COMPROBADO:** observación directa del árbol, del diferencial o de una comprobación estática.
- **NO VERIFICADO:** comportamiento que requeriría compilación o ejecución y queda diferido.

## Propósito

M01B separa del coordinador de construcción MIR la importación de layouts físicos. El cambio es un refactor
interno por movimiento: no modifica los algoritmos que publican layouts de structs, tuples, clases, futures o
iterators; sólo les proporciona una frontera privada propia y un translation unit independiente.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Repositorio y rama | `A:\nsc-devel`, rama `development`. |
| Naturaleza | Refactor interno no funcional por extracción física. |
| Objetivo exacto | Extraer el tramo de importación de layouts de `ProjectOopMirModel::build` a `Frontend/MIR/MirLayoutImporter.cpp`. |
| Oráculo | Comportamiento congelado de NSC 3.0.4 y estado inmediatamente anterior a M01B. |
| Contratos preservados | Sintaxis, semántica, API, ABI, NSMETA, diagnósticos, orden, ownership, formatos, artefactos y CLI. |
| Archivos de producto autorizados | `ProjectOopMirModel.h`, `MirBuildCoordinator.cpp`, `MirLayoutImporter.cpp` y las dos listas CMake que compilan MIR. |
| Archivo documental autorizado | Este expediente. |
| Archivos protegidos | Pruebas, fixtures, consumidores y, expresamente, `tests/unit/ProjectMethodBodyModelTests.cpp`. |
| Validación autorizada ahora | Gates estáticos de diferencial, referencias, identidad del bloque movido, whitespace y stage. |
| Validación prohibida ahora | Compilación, pruebas focales, baseline, suite integral, deploy e instalación. |
| Red y unidades | Sin `push`, `pull` ni `fetch`; todas las operaciones controladas permanecen en `A:`. |
| Herramientas | Sólo herramientas del PATH nativo de Windows; sin MSYS, worktrees ni workspaces auxiliares. |
| Criterio de terminación | Bloque completo movido una sola vez, llamada en la posición original, referencias CMake exactas, diferencial aislado y commit local propio. |

## Presupuesto y frontera causal

La entrada es la llamada vigente a `ProjectOopMirModel::build`. La operación extraída comienza después de fijar
las referencias internas del modelo y termina antes de dimensionar `functionByMember_`. Sus únicas salidas son
`structLayouts_`, `classLayouts_` y, ante error, el texto ya existente en `error` con retorno `false`.

No forman parte de M01B:

- tablas de despacho e interfaces;
- lowering de funciones o nodos;
- evaluación constante;
- verificación MIR;
- cambios algorítmicos o eliminación de recursión;
- caché, metadata o cualquier contrato público.

## Diseño mecánico

`ProjectOopMirModel` recibe un método privado `importLayouts`. `build` lo llama exactamente donde residía el
bloque original y retorna `false` si el importador conserva uno de sus diagnósticos. El helper
`abiPassingForSize`, usado exclusivamente por ese bloque, acompaña al movimiento. Los cuerpos de los loops,
las lambdas, el orden de publicación y los textos diagnósticos permanecen sin cambios funcionales.

## Gates estáticos requeridos

- comprobar que el bloque fuente desaparece del coordinador y existe una sola vez en el importador;
- comparar una representación normalizada del bloque antes y después del movimiento;
- comprobar una referencia por lista CMake al nuevo translation unit;
- comprobar cero cambios staged en pruebas y archivos concurrentes protegidos;
- ejecutar `git diff --check` y revisar el diferencial completo;
- conservar hashes, conteos y resultado de detección de movimiento en este expediente.

## Validación

`VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL`.

No se ejecutan compilación ni pruebas por instrucción vigente del usuario. Por tanto, M01B sólo puede afirmar
cierre estático del movimiento; build, regresión funcional y equivalencia de artefactos permanecen
**NO VERIFICADOS** hasta la tanda integral.

## Evidencia

- Bloque original: 329 líneas.
- Bloque extraído: 329 líneas.
- Diferencias línea por línea: 0.
- SHA-256 normalizado del bloque original:
  `9CF4A1189546A7891241B4BEDBD218089782C0BC3A304736345B4567B676D6F5`.
- SHA-256 normalizado del bloque extraído:
  `9CF4A1189546A7891241B4BEDBD218089782C0BC3A304736345B4567B676D6F5`.
- SHA-256 del archivo `MirLayoutImporter.cpp`:
  `05589B0C9ADEAF7163E8C632D4D9CF5B9CBF528C3AA02B99AF43F3D5CAFAB314`.
- Tamaño final del coordinador: 4,855 líneas.
- Tamaño del importador: 367 líneas.
- Declaraciones privadas de `importLayouts`: 1.
- Llamadas desde el coordinador: 1.
- Definiciones en el importador: 1.
- Referencias CMake al nuevo translation unit: 2, una por cada integración autorizada.
- `git diff --check`: sin diagnósticos.

La igualdad de conteo y hash del bloque demuestra identidad textual de los cuerpos, lambdas, loops,
diagnósticos y orden movidos. La nueva frontera privada sólo agrega el retorno exitoso explícito y la llamada
desde la posición original. Esta evidencia no sustituye compilación ni pruebas.

## Estado final de validación

- Auditoría estática del movimiento: aprobada.
- Auditoría de referencias de build: aprobada.
- Auditoría de archivos protegidos en stage: aprobada; seis rutas M01B y cero cambios concurrentes incluidos.
- Compilación: no ejecutada.
- Pruebas: no ejecutadas.
- Equivalencia de artefactos: no ejecutada.
- Rendimiento: no medido.

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
