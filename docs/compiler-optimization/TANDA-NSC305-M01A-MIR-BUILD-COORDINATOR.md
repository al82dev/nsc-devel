# NSC 3.0.5 - M01A - extracción física de `MirBuildCoordinator`

## Estado

`IMPLEMENTADO POR MOVIMIENTO PURO; VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL`.

No se ejecutaron builds ni pruebas en esta hoja por instrucción expresa del usuario. La evidencia de esta
hoja es estática y no se presenta como prueba funcional, de compilación ni de rendimiento.

## SCOPE LOCK

- Objetivo autorizado: extraer físicamente el translation unit MIR monolítico hacia
  `src/Frontend/MIR/MirBuildCoordinator.cpp` como primera frontera de M01.
- Naturaleza: refactor interno no funcional por movimiento puro.
- Revisión inicial: `dc21f2d4b0127305a6e7a7c085a112a13e479f39`, rama `development`.
- Oráculo: contenido byte por byte de `src/Frontend/ProjectOopMirModel.cpp` en la revisión inicial.
- Rango causal: exclusivamente esta hoja M01A.
- Archivos de producto autorizados:
  - `src/Frontend/ProjectOopMirModel.cpp`, sólo como origen del movimiento;
  - `src/Frontend/MIR/MirBuildCoordinator.cpp`, sólo como destino idéntico;
  - `CMakeLists.txt`, sólo para sustituir la ruta del source;
  - `tests/compiler-suite/CMakeLists.txt`, sólo para sustituir la misma ruta del source.
- Documento autorizado: este expediente.
- Comportamiento protegido: sintaxis, semántica, API, ABI, NSMETA, diagnósticos, formatos,
  serialización MIR, orden de construcción, paralelismo, determinismo, artefactos y consumidores 3.0.4.
- Archivos protegidos: headers públicos, pruebas, fixtures, manifests, runtime, linker, LLVM,
  `tests/unit/ProjectMethodBodyModelTests.cpp`, el expediente concurrente H02S29 y el tracking externo.
- Acciones prohibidas: cambiar algoritmos, nombres, firmas, includes, namespaces, cuerpos, orden de
  statements, flags, toolchain o configuración funcional; baseline, batería completa, deploy,
  instalación, push, pull y fetch.
- Validación autorizada en esta hoja: auditoría estática de identidad, referencias y diferencial.
- Criterio de terminación: el archivo destino es byte por byte idéntico al origen, no queda ninguna
  referencia de build activa a la ruta retirada, el diff sólo contiene el movimiento, las dos
  sustituciones de ruta y este expediente, y el commit no incorpora trabajo ajeno.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | Frontend MIR |
| Símbolos | Ninguno cambia; todos se trasladan sin edición |
| Comportamiento | Ninguno |
| Configuración | Sólo las dos rutas de source CMake |
| Pruebas | Ninguna ejecución en M01A |
| Perfiles | Ninguno |

## Evidencia previa

- Archivo origen: `src/Frontend/ProjectOopMirModel.cpp`.
- Tamaño: 266,864 bytes.
- Líneas: 5,189.
- SHA-256: `F68AB3CB92A37154FB2134D54EAFED0635BCC89C5E4A5CA73CA114D6135840F8`.
- Integración principal: `CMakeLists.txt` incluía el origen en `nsc_frontend`.
- Integración del compiler-suite: `tests/compiler-suite/CMakeLists.txt` incluía el mismo origen.

## Movimiento realizado

El archivo completo se trasladó a `src/Frontend/MIR/MirBuildCoordinator.cpp`. No se modificó una sola
línea de C++. La ruta nueva establece la primera frontera física de M01; las extracciones cohesionadas de
layouts, funciones, máquina de nodos, evaluación constante y verificación siguen perteneciendo a M01B-M01F.

Las dos listas CMake sustituyen exclusivamente la ruta antigua por la nueva. No cambian targets, orden de
sources, opciones, dependencias, generador, toolchain ni configuraciones.

## Contratos e invariantes

1. El translation unit conserva exactamente los mismos includes, helpers, símbolos y definiciones.
2. El target `nsc_frontend` compila el mismo contenido desde una ruta distinta.
3. El host directo del compiler-suite consume el mismo archivo trasladado.
4. No cambia ninguna declaración del header `ProjectOopMirModel.h`.
5. No cambia la representación ni la serialización MIR.
6. No cambia el orden ni el modelo de paralelismo de `ProjectOopMirModel::build`.
7. No se introduce recursión, allocation, branch ni capa runtime nueva.

## Complejidad y rendimiento

El movimiento no cambia complejidad temporal, espacial, allocations, locality ni scheduling de ejecución.
Su efecto esperado se limita a habilitar la descomposición física posterior de M01. No se reclama todavía
una mejora de tiempo de compilación del producto ni del propio compilador.

## Auditoría estática requerida

- comparar SHA-256 y número de líneas entre origen previo y destino;
- verificar que Git detecta un rename con contenido idéntico;
- comprobar cero referencias activas de CMake a la ruta retirada;
- ejecutar `git diff --check`;
- revisar que el stage no contenga los cambios concurrentes protegidos.

## Validación diferida

`VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL`.

No se ejecutaron compilación, pruebas focales, baseline ni suite completa. Por tanto, esta hoja sólo puede
afirmar que el movimiento y su integración fueron auditados estáticamente. La compilación y la regresión
funcional permanecen pendientes de la tanda integral autorizada por el usuario.

## Resultado de la auditoría estática

- SHA-256 del destino: `F68AB3CB92A37154FB2134D54EAFED0635BCC89C5E4A5CA73CA114D6135840F8`.
- Líneas del destino: 5,189.
- Blob Git del origen en `HEAD`: `04ef6746f58cf7d2df9e119372fbffd3918f5d08`.
- Blob Git del destino: `04ef6746f58cf7d2df9e119372fbffd3918f5d08`.
- Detección Git: `R100`, cero líneas añadidas o retiradas en el translation unit.
- Referencias CMake activas a la ruta anterior: cero en las dos integraciones autorizadas.
- Referencias CMake activas a la ruta nueva: dos, una por cada integración autorizada.
- `git diff --cached --check`: sin diagnósticos.
- Stage: cuatro entradas lógicas de M01A; cero archivos concurrentes incluidos.

La igualdad de blobs demuestra identidad textual exacta del C++. No sustituye la compilación ni las pruebas
diferidas.

## Elementos no verificados

- Compilación del target `nsc_frontend`: no ejecutada.
- Compilación del host compiler-suite: no ejecutada.
- Pruebas MIR/O0: no ejecutadas.
- Equivalencia de artefactos producidos: no ejecutada.
- Rendimiento: no medido.

## Siguiente frontera

M01B podrá extraer `MirLayoutImporter` desde este translation unit mediante otra hoja y otro commit. Esa
extracción no forma parte de M01A y no se adelanta aquí.
