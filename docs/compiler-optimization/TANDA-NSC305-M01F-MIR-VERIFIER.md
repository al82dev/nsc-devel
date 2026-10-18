# Tanda NSC 3.0.5 M01F — extracción de `MirVerifier`

Fecha: 2026-10-18
Rama: `development`
Revisión de partida: `677f2f1648c467739f36d306b933e2b29c136ed7`

## SCOPE LOCK

| Dimensión | Contrato de la tanda |
|---|---|
| Objetivo | Extraer físicamente la finalización y verificación del MIR desde `MirBuildCoordinator.cpp` hacia `Frontend/MIR/MirVerifier.cpp`. |
| Naturaleza | Refactor por movimiento puro. |
| Conducta congelada | Orden de catálogos genéricos, bindings de interfaz, validaciones Item398, resolución de `PanicHandler`, diagnósticos, API, ABI, NSMETA, MIR y dumps. |
| Archivos de producto autorizados | `ProjectOopMirModel.h`, `MirBuildCoordinator.cpp`, `MirVerifier.h` y `MirVerifier.cpp`. |
| Infraestructura autorizada | Una entrada en `nsc_frontend` y una entrada espejo en `nsc_component_tests`. |
| Archivos protegidos | Parser/R01E, `ProjectMethodBodyModelTests.cpp`, LLVM, HIR, fixtures y expectativas existentes. |
| Prohibido | Cambiar semántica, corregir pruebas adyacentes, migrar algoritmos M02, ejecutar baseline o batería completa. |
| Terminación | Doble registro válido, cero funciones mayores de 60 ejecutables, build productivo y host directo PASS, focos Item398/Stage37/PanicHandler PASS, diff auditado y commit local aislado. |

## Frontera extraída

El tipo privado `ProjectOopMirModel::MirVerifier` conserva la última secuencia histórica de
`ProjectOopMirModel::build`:

1. registra especializaciones de funciones;
2. registra especializaciones de campos estáticos;
3. registra especializaciones de tablas de dispatch;
4. valida y publica bindings de interfaz;
5. limpia, resuelve y valida el `PanicHandler` configurado.

El coordinador conserva una sola llamada `finalize(error)` en la posición original, antes de marcar el modelo
como construido y antes de persistir el caché MIR. No se movieron ni cambiaron las validaciones distribuidas
que pertenecen a la construcción de layouts, lowering, campos estáticos u otras fases.

Este tipo pertenece a `nativesharp::frontend` y no sustituye ni modifica el verificador SSA ya existente en
`src/MIR/MirVerifier.cpp` bajo `nativesharp::mir`.

## Identidad y tamaño

Los cinco diagnósticos observables del bloque histórico aparecen exactamente una vez antes y una vez después
del movimiento:

- tres diagnósticos Item398 de vectores, identidad ABI y binding duplicado;
- `PanicHandler` que no nombra una función de namespace;
- firma inválida del `PanicHandler`.

La extracción define un constructor y siete métodos. La medición conservadora de los siete cuerpos no
triviales produjo:

- máximo de 33 líneas físicas no vacías;
- cota superior de 24 líneas ejecutables;
- cero funciones por encima de 60 líneas ejecutables.

La separación en helpers conserva el orden original y no introduce un algoritmo nuevo: cada helper contiene
una subsecuencia contigua del bloque histórico y `finalize` las invoca en el mismo orden.

## Infraestructura

No se creó prueba, caso, target, `add_test`, catálogo, manifiesto ni ruta default. La fuente nueva queda
integrada por:

1. `CMakeLists.txt`: una entrada exacta en `nsc_frontend`;
2. `tests/compiler-suite/CMakeLists.txt`: una entrada exacta en `nsc_component_tests`;
3. `validate_source_registration.py`: PASS, exit 0;
4. build directo del host `nsc_component_tests`: PASS.

## Validación focal

Todos los temporales, proyectos contrafactuales y logs se conservaron en `A:\temp`.

| Gate | Resultado | Evidencia |
|---|---|---|
| Build `nsc` | PASS, exit 0, 35.299 s | `A:\temp\m01f-build-nsc.log` |
| Build `nsc` final sobre HEAD y parser actual | PASS, exit 0, 19.821 s | `A:\temp\m01f-final-nsc-build.log` |
| Build Item398 + Stage37 + NS3-T07 | PASS, exit 0, 14.202 s | `A:\temp\m01f-build-focal-tests.log` |
| Build `nsc_component_tests` | PASS, exit 0, 34.634 s | `A:\temp\m01f-build-component-tests.log` |
| CTest Item398 | PASS 1/1, 0.12 s | `A:\temp\m01f-ctest-focal.log` |
| CTest Stage37 | PASS 1/1, 0.32 s | `A:\temp\m01f-ctest-focal.log` |
| `PanicHandler` válido | PASS, exit 0; marker MIR presente | `A:\temp\m01f-panic-valid.log` |
| `PanicHandler` inválido | PASS negativo, exit 1; diagnóstico exacto presente | `A:\temp\m01f-panic-invalid.log` |
| NS3-T07 agregado | PASS 58/58; CTest PASS 1/1 | `A:\temp\nsmeta-f01-direct.log`; `A:\temp\nsmeta-f01-ctest.log` |
| Registro de fuentes final | PASS, exit 0 | `A:\temp\m01f-source-registration-final.log` |

Item398 prueba el catálogo de una especialización genérica cerrada, la identidad ABI estable, los vectores de
slots y callee, la búsqueda del binding y el consumo LLVM. Stage37 prueba el perfil de pánico y el flujo de
inicialización estática. Los dos proyectos CLI prueban directamente la resolución positiva y el diagnóstico de
firma inválida del handler.

La caída histórica de `nsc.ns3.t07.errors` fue aislada fuera de M01F: el planificador LLVM
desreferenciaba slots MIR ausentes y, después de corregirlo, una expectativa de prueba conservaba el literal
NSMETA 1.38 frente al minor canónico actual. Ambos defectos tienen commits y expedientes separados. Con esas
causas cerradas, el agregado pasó directamente 58/58 checks y su CTest pasó 1/1 usando
`A:\nslink-devel\bin\release\nslink.exe`.

## Resultado

M01F reduce la responsabilidad física de `MirBuildCoordinator.cpp`, conserva el orden y los diagnósticos del
bloque histórico, cierra la infraestructura doble y dispone de evidencia focal directa para cada contrato
extraído. No se modificó sintaxis, semántica, ABI, NSMETA, formato MIR ni algoritmo M02.
