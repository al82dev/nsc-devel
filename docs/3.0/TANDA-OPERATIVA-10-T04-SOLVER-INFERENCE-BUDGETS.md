# Tanda operativa 10 — T04 constraint solver, inferencia y budgets

> **Actualización 2026-10-03:** `NS3-T04-F04` cerró la deuda contextual original. El solver ahora
> propaga tipos exactos a llamadas anidadas, condicionales y tuples; usa parámetros escritos de
> lambdas como evidencia y mantiene method groups/lambdas sin tipo bajo un destino delegate único.
> El contrato vigente está en `docs/3.0/language/bidirectional-inference.md`.

## Objetivo

Conectar al compilador real tres piezas de T04 que no podían quedarse como helpers aislados:

1. usar el modelo declarativo de constraints después de una inferencia contextual;
2. inferir argumentos genéricos desde el tipo esperado sin abrir búsqueda de conversiones;
3. garantizar que la monomorfización termina y deja un informe auditable.

La entrega debía compilar un programa NativeSharp 3.0 hasta PE Windows x64 O0, conservar el camino
de especialización y constraints ya usado por bibliotecas 2.x, y rechazar explosiones genéricas con
causa estable. No incluye const generics, packs, opaque aliases ni partial specialization: esos son
la tanda operativa 11.

## Situación encontrada

- `ProjectGenericModel` ya materializaba tipos y métodos, compartía una cache canónica y transportaba
  templates por NSMETA.
- `ProjectGenericConstraintModel` ya comprobaba `class`, clase base e interfaces, incluidos
  productores precompilados y grupos de overloads.
- La inferencia local sólo unificaba tipos de argumentos. Una llamada sin evidencia allí —por
  ejemplo `int x = Make();` o `Item x = Identity(null)`— fallaba aunque el contexto fuera exacto.
- Las especializaciones se insertaban en cache antes de recorrer el clon, lo que detenía recursión
  de la misma clave, pero una serie de claves siempre nuevas (`Expand<int[]>`, `Expand<int[][]>`,
  ...) podía crecer sin límite.
- `language::InstantiationBudget` existía y tenía prueba aislada, pero el materializador productivo
  nunca lo llamaba.
- El dump listaba templates y especializaciones, pero no cantidad materializada, reutilizaciones,
  profundidad ni límites.

## Diseño aplicado

### Inferencia bidireccional acotada

`inferGenericCall` acepta ahora un tipo esperado opcional. Cada candidata unifica:

- toda evidencia local conocida de sus argumentos;
- su tipo de retorno con el tipo esperado exacto, si existe.

Una expresión sin tipo local —en particular `null`— no se trata como contradicción. Puede quedar
resuelta por el retorno esperado. En cambio, evidencia conocida incompatible sigue descartando la
candidata: `long x = Identity(1)` no hace que este solver temprano adivine una conversión `int→long`.
La conversión final continúa bajo el modelo semántico ordinario.

El contexto se propaga en tres lugares deterministas:

- inicializador directo de una local con tipo explícito;
- llamada directa en `return`, usando el retorno de la función o método;
- llamada directa en el lado derecho de una asignación, usando el tipo local conocido del destino.

El tipo se clona antes de que la monomorfización reemplace `Box<int>` por su nombre físico. No se
hace una búsqueda global, no se prueban conversiones arbitrarias y no se multiplica el conjunto de
candidatas. `var x = Make()` continúa fallando porque no ofrece evidencia.

### Presupuesto conectado al materializador

Cada cache miss de tipo o método entra en un único `InstantiationBudget`; cada cache hit sólo
incrementa el contador de reutilización. Los límites de compilador son:

- profundidad activa máxima: 64;
- especializaciones nuevas máximas: 16384.

Las especializaciones importadas ya compiladas no consumen el presupuesto local. Una entrada que
rebasa el límite no se inserta parcialmente en la cache. El diagnóstico apunta al sitio de
instanciación e incluye hasta los últimos doce marcos de la cadena causal.

`InstantiationBudget` conserva ahora cantidad, profundidad máxima observada y stack activo. El
modelo publica `GenericInstantiationReport`, y `--dump-project-generics` añade:

- `materialized`;
- `cache-hits`;
- `maximum-depth`;
- `depth-limit`;
- `count-limit`.

El orden sigue siendo el orden determinista de recorrido/materialización que ya gobernaba las
claves y el dump.

### Constraints productivos

No se escribió un segundo solver. La inferencia sólo elige los argumentos concretos; después,
`ProjectGenericConstraintModel` ejecuta exactamente la misma satisfacción declarativa que para una
especialización explícita o rehidratada. El testigo `ConstrainedDefault<T>() where T : class,
IValue` demuestra la ruta positiva; un `Bad` inferido demuestra que el error causal sigue apuntando
al argumento y al contrato.

## Cambios por archivo

### Modificados

- `include/nativesharp/Language/GenericSemantics.h`
  - expone límites, profundidad máxima y stack activo del presupuesto.
- `src/Language/GenericSemantics.cpp`
  - separa errores de profundidad/cantidad/recursión;
  - construye una cadena causal acotada y determinista.
- `include/nativesharp/Frontend/ProjectGenericModel.h`
  - define límites por defecto y `GenericInstantiationReport`;
  - transporta el retorno esperado en el scope genérico;
  - conecta el presupuesto y cuenta cache hits.
- `src/Frontend/ProjectGenericModel.cpp`
  - captura tipos esperados antes de la reescritura física;
  - combina evidencia obtenida y esperada;
  - aplica el presupuesto únicamente a cache misses;
  - deja la cache limpia cuando una entrada es rechazada.
- `src/Frontend/ProjectGenericModelDumper.cpp`
  - publica el informe de instanciaciones en el encabezado del dump.
- `tests/unit/GenericInferenceTests.cpp`
  - mueve retorno-only y `Identity(null)` al conjunto positivo contextual;
  - conserva como negativo el caso `var` sin evidencia.
- `tests/unit/NativeSharp3T04GenericTests.cpp`
  - comprueba mensajes causales, profundidad observada y límite de cantidad independiente.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c60 para el baseline final que ejecutará Alberto.
- `docs/3.0/TANDA-04-GENERICOS-3.0.md`
  - reemplaza la descripción de helper aislado por el estado productivo y sus límites reales.

### Nuevos

- `tests/cc1/c60_ns3_generic_solver/c60_ns3_generic_solver.nsproj`
  - proyecto edition 3 mínimo.
- `tests/cc1/c60_ns3_generic_solver/Main.ns`
  - cubre inicialización, retorno, asignación, `null`, constraints y cache; devuelve 42.
- `docs/3.0/TANDA-OPERATIVA-10-T04-SOLVER-INFERENCE-BUDGETS.md`
  - evidencia y handoff de este commit.

No se creó código fuente fuera de `A:\nsc-devel`. Los probes, ejecutables y logs temporales se
eliminaron antes del commit.

## Mejora concreta

- Se elimina la obligación artificial de escribir `<T>` cuando el contexto exacto ya determina T.
- `null` puede participar en inferencia sin fingir que tiene un tipo propio.
- Constraints explícitos e inferidos comparten una sola ruta semántica.
- Una familia de instanciaciones siempre nuevas ya no puede agotar stack/memoria indefinidamente.
- La cantidad de trabajo genérico y la eficacia de cache son visibles en la CLI.
- El fallo por recursos es reproducible, localizado y explica la cadena que lo causó.

## Validación dirigida ejecutada

1. Construcción únicamente de `nsc`, configuración Debug/O0, target `nsc`: correcta.
2. `c60_ns3_generic_solver` con `--emit-project-oop-exe -O0`: correcta; proceso termina en 42.
3. Dos ejecuciones de `--dump-project-generics` sobre c60:
   - contenido genérico idéntico;
   - `templates=4`, `specializations=4`, `materialized=4`, `cache-hits=3`;
   - `maximum-depth=1`, `depth-limit=64`, `count-limit=16384`.
4. Cuatro probes negativos individuales, todos con exit distinto de cero:
   - `var value = Make()`: no existe tipo esperado ni argumento inferible;
   - `long value = Identity(1)`: evidencia obtenida y esperada incompatible;
   - `Bad value = Make()` con `where T : IValue`: constraint causal incumplido;
   - `Expand<T[]>`: detención en profundidad 64 con cadena causal.
5. El target unitario T04 no estaba generado por la configuración `build-clang`; se intentó por
   nombre y Ninja respondió `unknown target`. No se reconfiguró el árbol ni se ejecutó CTest.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Límites y dependencias explícitas

- F04 cerró lambdas tipadas, tuples contextuales, destino único para method groups/lambdas sin tipo
  y contexto propagado a llamadas genéricas anidadas. No se infiere mediante conversiones.
- Const generics, packs, opaque aliases y partial specialization: tanda operativa 11.
- Configuración pública de budgets por `.nsproj`/CLI: candidata posterior; los límites actuales son
  contrato fijo y visible del compilador Windows x64.
- Superficie completa 3.0 de templates/constraints en NSMETA y medición de duplicación/tiempo:
  tandas 11, 18 y 30.

Por estas dependencias, la tanda operativa 10 queda cerrada; T04 maestro permanece `IMPLEMENTING`.
