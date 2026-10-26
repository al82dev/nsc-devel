# Congelamiento 3.0 — análisis de `await` síncrono sin recursión mutua

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `88c1246c4`.

## Propósito y problema

C-02 agregó `ProjectMethodBodyModel::verifySynchronousAwaits`, que rechaza un `await` síncrono cuyo productor
puede suspenderse (`SynchronousAwaitCanSuspend`). Para decidirlo, `awaitOperandCanSuspend` y
`awaitProducerCanSuspend` (`src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp`) se llamaban mutuamente: una
cadena larga de productores async consumía pila del compilador en proporción a su profundidad, y los lineamientos
prohíben la recursión. Instrucción de Alberto Huerta Luna (2026-10-26): eliminar la recursión mutua de await sin
cambiar su resultado semántico.

## SCOPE LOCK

- **Objetivo.** Las dos funciones quedan sin recursión, con el mismo resultado para todo programa.
- **Naturaleza.** Refactor sin cambio de semántica.
- **Debe quedar idéntico.** El orden de examen (awaits en orden de fuente, alto en el primero que se suspende), el
  memo (`0` en visita leído como «no se suspende», `1` no, `2` sí), los diagnósticos y sus posiciones, y las
  firmas declaradas en `ProjectMethodBodyModel.h`.
- **Modificable.** `src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp`.
- **Prohibido.** Cambiar la clasificación de algún productor; corregir aquí otros defectos del análisis.
- **Terminación.** Corpus de abajo idéntico antes y después.

## Cambio

- `awaitOperandOrigin` (función libre): sustituye un local por su inicializador único y `it.MoveNextAsync()` por el
  origen de `it`, en un bucle. Si el origen vuelve a un nodo ya visitado es origen desconocido.
- `boundAwaitProducer`: el productor de un binding resuelto, o ninguno.
- `awaitProducerCanSuspend` recorre en profundidad con una pila explícita de `AwaitProducerFrame` (ordinal,
  cuerpo, awaits, siguiente, suspende). Abre un frame por productor analizable; un productor no analizable es «se
  suspende» sin frame, como antes.
- `awaitOperandCanSuspend` resuelve el origen y delega en `awaitProducerCanSuspend`; ya no hay llamada de vuelta.

`ProjectOopHirModel::awaitProducerCanSuspend` (`HirAsyncLowering.cpp`) ya era iterativo y no cambia.

## Validación

Corpus en `A:\temp\nsc-dec\aw`: los 40 programas de `Freeze30ContractTests.cpp` que contienen `await` o
`MoveNextAsync` (con su preludio), los tres rechazos de `testSynchronousAwaitRejected` (`r1..r3`) y ocho casos
nuevos (`k1..k8`: ciclo mutuo, ciclo con productor que suspende en dos órdenes de declaración, cadena, auto
recursión, generador que espera a un productor que suspende, iterador reasignado y un iterador inicializado consigo
mismo). Cada uno se compiló como ejecutable con el nsc anterior y con el nuevo; se compararon código de salida,
líneas de error y presencia del ejecutable.

Resultado: 50 de 51 idénticos. La única diferencia es `k8`
(`AsyncIterator<int, int> it = it.MoveNextAsync();`): el nsc anterior moría por desbordamiento de pila
(`0xC00000FD`) en la recursión; el nuevo reporta `unknown identifier 'it'` y el rechazo de C-02 (origen
desconocido). No es un cambio de clasificación: antes no había resultado.

## Hallazgo (no corregido aquí)

`k2` y `k3` son el mismo programa con dos callables síncronos en distinto orden. `A` y `B` se esperan
mutuamente y `B` además espera a `P`, que se suspende; así `A` puede suspenderse. En `k2` (`First` espera a `B`
antes que `Main` espere a `A`) solo se rechaza `First`: al abrir `B` se visita `A`, que lee a `B` «en visita»
como «no se suspende» y queda memorizado como `1`; luego `Main` acepta `await A(3)`. En `k3` se rechazan los dos.
Es un falso negativo del análisis de C-02 que depende del orden, igual antes y después de este cambio; en
ejecución termina en el pánico de DEC-04. Se corrige con el análisis por alcanzabilidad junto con D-04.

## No verificado

- `Freeze30ContractTests` (unitarias) y batería completa: cierre final.
