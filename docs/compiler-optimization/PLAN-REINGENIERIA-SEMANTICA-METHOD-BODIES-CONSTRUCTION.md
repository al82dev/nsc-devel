# Plan de reingenieria semantica de `method-bodies` y `construction`

**Producto:** NativeSharp Compiler 3.0

**Plataforma:** Windows x64

**Configuracion prioritaria:** O0

**Estado:** plan de implementacion aprobado; codigo aun no implementado

**Fecha de corte:** 2026-10-05

## 1. Proposito

Este documento fija la raiz comprobada del tiempo excesivo de compilacion y el plan para corregirla. No es una
coleccion de microoptimizaciones. Define una reingenieria del analisis semantico previo a LLVM, preservando los
contratos observables del compilador y preparando la aplicacion real de `-j N` al trabajo que hoy domina O0.

El desbordamiento de pila de expresiones profundas ya fue corregido mediante recorridos iterativos respaldados por
memoria dinamica. Esa correccion elimina un fallo de robustez, pero no resuelve la causa principal de la lentitud.

## 2. Evidencia de la compilacion real

La reproduccion O0 de `NativeSharp.Math.Functions` termino correctamente y produjo `.lib`, `.nsmeta` y `.h`.

| Etapa | Tiempo observado | Lectura tecnica |
|---|---:|---|
| `method-bodies` | 2,339,082 ms, aproximadamente 39 minutos | Cuello de botella dominante |
| `construction` | 409,282 ms, aproximadamente 6.8 minutos | Segundo cuello de botella |
| `llvm-object-generation` | 31,048 ms, aproximadamente 31 segundos | LLVM no domina esta compilacion |

Artefactos producidos por la reproduccion:

| Artefacto | Tamano | SHA-256 |
|---|---:|---|
| Biblioteca | 25,437,566 bytes | `29B285B2DF35DD0B554D86275F40A2DF2E9460001BE57D7AF0329AA1B31158FA` |
| Metadata | 9,893,332 bytes | `D9255448B868450EF6016444AA85266529A01D089162016B30178CB3E4CD7F78` |
| Encabezado | 1,771 bytes | `E47490EBF4648311CDFF8F19C996F14BBDC7A89D0A0CC90715E2225FB9A13C13` |

La conclusion es directa: optimizar solamente la emision LLVM no solucionaria el problema. La mayor parte del
tiempo se consume antes, en el modelo de cuerpos de metodos y en el modelo de construccion.

## 3. Raiz del problema

### 3.1 Analisis de callables esencialmente serial

`ProjectMethodBodyModel::build`, en
`src/Frontend/ProjectMethodBodyModel.cpp`, construye una lista de callables y los analiza uno por uno. En la
reproduccion se observaron mas de ocho mil ordinales de callable. La opcion `-j 12` beneficia etapas posteriores,
pero no distribuye este trabajo semantico dominante.

### 3.2 Busquedas lineales repetidas sobre colecciones globales

Durante el analisis de expresiones se consultan repetidamente vectores globales completos o grandes subconjuntos:

- tipos de expresion;
- enlaces de llamadas;
- enlaces de nombres;
- tipos nombrados;
- miembros por tipo y nombre;
- constructores por tipo;
- informacion de getters, setters e indexadores;
- propiedades derivadas de tipos y agregados.

Una busqueda lineal aislada es barata. Miles de callables, expresiones y consultas anidadas convierten ese patron en
trabajo superlineal. El compilador conserva vectores utiles para salida determinista, pero actualmente tambien los
usa como indices de consulta.

### 3.3 Propagacion de escape por punto fijo global

`finalizeEscapeSummaries` vuelve a recorrer todas las llamadas y todas las llamadas a funciones de namespace en
cada iteracion del punto fijo. Aunque solo cambie el resumen de un callable, se reevalua el grafo completo.

### 3.4 Estado de ownership costoso de copiar

El flujo de ownership utiliza conjuntos y mapas hash dentro de instantaneas. Las ramas, uniones de flujo y sondeos
de codigo no alcanzable pueden copiar y restaurar estructuras completas. El costo depende de la cantidad de estado
vivo, no solamente de los valores modificados por la rama.

### 3.5 `construction` vuelve a recorrer el AST y repite busquedas globales

`ProjectConstructionModel::build`, en `src/Frontend/ProjectConstructionModel.cpp`, realiza otro recorrido amplio del
AST despues de `method-bodies`. Ademas:

- `scanUnsupportedContexts` y `scanNode` son recursivos;
- cada construccion vuelve a resolver tipos de argumentos;
- `expressionType` busca linealmente en todos los enlaces de tipo de expresion;
- la resolucion de constructores carece de un indice especializado suficiente.

El patron mas preocupante es, conceptualmente:

`construcciones x argumentos x enlaces globales de expresion`

Esto explica que `construction` pueda consumir minutos aun despues de haber realizado el analisis semantico de los
cuerpos.

### 3.6 Ausencia de aislamiento y reutilizacion semantica

El estado mutable esta concentrado en modelos globales. Eso dificulta:

- paralelizar callables;
- fusionar resultados en un orden determinista;
- reutilizar resultados por unidad o callable;
- invalidar solamente lo que cambio.

La cache incremental es deseable, pero no debe construirse encima de un modelo global no aislado. Primero hay que
reducir complejidad, separar estado y hacer determinista la fusion.

## 4. Principios obligatorios

1. No se introducira recursion nueva. Los recorridos profundos se implementaran con pilas o colas explicitas en
   memoria dinamica.
2. Los vectores actuales se conservaran cuando formen parte del orden de salida, dumps, metadata o diagnosticos.
3. Los indices laterales aceleraran consultas; no redefiniran el orden observable.
4. La paralelizacion no podra cambiar el primer diagnostico, su texto, los ordinales ni la serializacion.
5. O0 es la configuracion prioritaria y la unica matriz de rendimiento durante estas tandas.
6. No se ejecutara baseline entre tandas. El baseline completo queda reservado al cierre y lo ejecuta el usuario.
7. Cada tanda tendra un commit propio y un documento de cambios, archivos, pruebas y evidencia.
8. Cada cambio se validara con pruebas dirigidas; no se disfrazaran minibaselines como pruebas puntuales.

## 5. Arquitectura objetivo

El flujo final sera:

1. construir catalogos e indices semanticos inmutables;
2. identificar callables y sus dependencias iniciales;
3. analizar callables en contextos mutables aislados;
4. producir un resultado autocontenido por callable;
5. fusionar resultados por ordinal estable;
6. propagar resúmenes mediante colas de dependencias afectadas;
7. entregar sitios de construccion ya tipados al modelo de construccion;
8. emitir metadata y codigo usando los mismos contratos deterministas actuales.

Dos estructuras delimitan la separacion:

```text
ProjectCallableAnalysisContext
  - scopes and locals
  - expression, name and call bindings
  - ownership flow
  - diagnostics
  - discovered dependencies and specialization requests

CallableAnalysisResult
  - stable callable ordinal
  - immutable semantic bindings
  - ownership and escape summary
  - construction sites
  - deterministic diagnostics
  - discovered work for the next analysis wave
```

Los nombres finales pueden ajustarse al codigo existente, pero la separacion de responsabilidades es obligatoria.

## 6. Tandas de implementacion

### Tanda 1. Indices semanticos de acceso O(1)

**Objetivo:** eliminar barridos repetidos de colecciones globales en consultas calientes.

Se agregaran indices laterales, como minimo, para:

- `(AstNode*, callableOrdinal) -> expression type binding`;
- `(AstNode*, callableOrdinal) -> call binding`;
- `(AstNode*, callableOrdinal) -> name binding`;
- `typeKey -> typeOrdinal`;
- `typeOrdinal -> propiedades precalculadas`;
- `(typeOrdinal, memberName) -> miembros candidatos`;
- `typeOrdinal -> constructores candidatos`.

Los indices se actualizaran al registrar cada binding. Las colecciones ordenadas existentes seguiran siendo la
fuente de serializacion y dumps. Antes de reemplazar una busqueda inversa debe comprobarse si la semantica exige
obtener el enlace mas reciente; en ese caso el indice preservara explicitamente esa regla.

**Criterios de cierre:**

- ninguna consulta migrada recorre el vector completo;
- orden de dumps y diagnosticos sin cambios;
- pruebas unitarias de colisiones por nodo entre distintos callables;
- prueba de reemplazo o enlace mas reciente, cuando aplique;
- medicion dirigida de contadores de consultas e impactos de cache.

### Tanda 2. `construction` consume resultados semanticos, no vuelve a descubrirlos

**Objetivo:** eliminar el segundo recorrido completo de cuerpos y las busquedas lineales por argumento.

El analisis de cuerpos registrara una lista de sitios de construccion:

```text
ConstructionSite
  - syntax node
  - caller ordinal
  - constructed type
  - typed arguments
  - source location
  - required construction policy
```

`ProjectConstructionModel` consumira esa lista. Solo conservara recorridos independientes cuando exista una regla
que no pueda validarse durante el analisis normal. Esos recorridos se implementaran con worklists iterativos.

**Criterios de cierre:**

- `scanNode` y `scanUnsupportedContexts` no usan recursion;
- no se vuelve a recorrer cada cuerpo para redescubrir `new`;
- cada tipo de argumento se obtiene por indice o viene incluido en el sitio;
- resolucion de constructores usa candidatos preindexados;
- los diagnosticos mantienen ubicacion, prioridad y orden;
- una compilacion O0 real de `NativeSharp.Math.Functions` mide la etapa por separado.

### Tanda 3. Estado compacto de ownership y flujo

**Objetivo:** hacer que copiar, bifurcar y fusionar estado dependa de cambios reales o de palabras densas, no del
tamano de mapas y conjuntos generales.

Cada entidad local recibira un ordinal denso por callable. Los estados booleanos se representaran con vectores de
bits o arreglos indexados. Los estados con payload usaran ranuras indexadas y tablas auxiliares solo cuando sean
necesarias.

Se evaluaran dos mecanismos para las ramas:

- copia bajo escritura de bloques de estado;
- diario de cambios con restauracion y fusion explicitas.

La eleccion se hara con mediciones de memoria y tiempo sobre un caso grande, no por preferencia teorica.

**Criterios de cierre:**

- sin mapas hash en los caminos mas calientes de estado local cuando exista ordinal denso;
- snapshot y restore sin reconstruccion completa innecesaria;
- uniones de flujo por operaciones deterministas;
- cobertura dirigida de ramas, loops, borrows, moves y codigo no alcanzable;
- consumo maximo de memoria medido antes y despues.

### Tanda 4. Propagacion de escape mediante worklist

**Objetivo:** sustituir el punto fijo que reescanea todas las llamadas por propagacion incremental dentro de la
compilacion actual.

Se construira el grafo inverso `callee -> dependientes`. Cuando cambie el resumen de escape de un callable, solo se
encolaran los callers o sitios afectados. Cada elemento estara como maximo una vez pendiente en la cola.

El dominio de resumen debe seguir siendo una reticula finita y monotona. Si hoy alguna transferencia puede reducir
un estado, se corregira primero el contrato antes de usar worklist.

**Criterios de cierre:**

- ningun ciclo global `while changed` recorre todas las llamadas;
- ciclos recursivos del grafo convergen;
- resultados independientes del orden de insercion;
- contadores muestran aristas procesadas y reencolamientos;
- los resúmenes finales coinciden con la implementacion anterior en fixtures dirigidos.

### Tanda 5. Contexto y resultado aislados por callable

**Objetivo:** retirar el estado mutable compartido que impide paralelizar con seguridad.

Cada callable se analizara dentro de un contexto propio. El analisis no escribira directamente en vectores globales.
Entregara un `CallableAnalysisResult`, que se fusionara por ordinal estable una vez completado.

Los diagnosticos conservaran una clave de orden total, por ejemplo:

1. unidad y posicion de fuente;
2. ordinal de callable;
3. fase y ordinal local del diagnostico.

**Criterios de cierre:**

- analizar dos callables independientes no comparte estado mutable;
- la fusion secuencial reproduce byte por byte dumps y metadata relevantes;
- el primer diagnostico permanece estable;
- una prueba ejecuta los resultados en orden de finalizacion invertido y obtiene la misma salida;
- solicitudes de especializacion se deduplican con claves estables.

### Tanda 6. Paralelismo por oleadas deterministas

**Objetivo:** aplicar `-j N` al analisis de cuerpos sin carreras ni resultados no deterministas.

El planificador operara por oleadas:

1. ordenar el conjunto listo por ordinal;
2. analizar los callables de la oleada con el numero de trabajos seleccionado;
3. recolectar resultados sin fusion dependiente del orden de terminacion;
4. fusionar por ordinal;
5. deduplicar y ordenar nuevas especializaciones o dependencias;
6. formar la siguiente oleada hasta cerrar el trabajo.

Los work items no dependeran de referencias mutables a contenedores que puedan realocarse. Los catalogos comunes
seran inmutables durante una oleada.

**Criterios de cierre:**

- `-j 1`, `-j 2` y `-j 12` producen salidas semanticamente identicas;
- los artefactos deterministas conservan hashes entre repeticiones equivalentes;
- no hay data races bajo las herramientas disponibles del proyecto;
- una compilacion O0 real de `NativeSharp.Math.Functions` muestra escalamiento medible;
- fallos y cancelacion detienen nuevas oleadas sin corromper el modelo.

## 7. Metas de rendimiento

Estas cifras son objetivos de ingenieria, no resultados ya obtenidos:

| Etapa | Punto de partida | Meta inicial |
|---|---:|---:|
| `construction` | 409 s | menos de 30 s |
| `method-bodies`, antes de paralelizar | 2,339 s | menos de 600 s |
| `method-bodies`, con `-j 12` | sin paralelismo efectivo | menos de 180 s |

No se aceptara una mejora basada en omitir validaciones semanticas. Si una meta no se alcanza, los contadores y
perfiles de la nueva arquitectura deben indicar el siguiente costo dominante sin volver a depender de conjeturas.

## 8. Instrumentacion requerida

Las mediciones deben ser baratas y estar desactivadas o resumidas fuera del modo de telemetria. Como minimo se
registraran:

- callables descubiertos, analizados y reutilizados;
- consultas a indices y fallos de indice;
- recorridos lineales residuales por categoria;
- sitios de construccion consumidos;
- snapshots, bytes copiados y fusiones de ownership;
- aristas de escape procesadas y reencolamientos;
- tiempo de analisis, fusion y espera por oleada;
- pico de memoria de las etapas semanticas.

La instrumentacion no podra cambiar orden, hashes, diagnosticos ni comportamiento de la compilacion.

## 9. Estrategia de validacion

### Durante cada tanda

1. implementar el cambio acotado;
2. construir solamente `nsc` y los blancos directamente necesarios;
3. ejecutar pruebas unitarias o fixtures especificos del mecanismo cambiado;
4. ejecutar al menos una compilacion O0 representativa cuando la tanda altere un camino real;
5. registrar tiempos, comandos, resultados y archivos en el documento de la tanda;
6. crear un commit atomico.

### Puntos de medicion grandes

- Despues de las tandas 1 y 2: una compilacion O0 de `NativeSharp.Math.Functions`.
- Despues de la tanda 6: una compilacion O0 final del mismo proyecto, con comparacion reproducible.
- Despues de cerrar todas las tandas: el usuario decide y ejecuta el baseline completo.

No se correran O1, O2 u O3 durante esta reingenieria salvo instruccion expresa. Tampoco se ejecutara el baseline de
forma automatica.

## 10. Riesgos y controles

| Riesgo | Control obligatorio |
|---|---|
| Un indice altera el binding anterior | Pruebas del enlace mas reciente y claves completas |
| La fusion paralela cambia el orden de diagnosticos | Clave total estable y fusion por ordinal |
| Los caches conservan punteros invalidados | Usar ordinales o almacenamiento con direccion estable |
| Ownership compacto pierde estados raros | Comparacion diferencial dirigida antes de retirar el modelo anterior |
| La worklist de escape no converge | Dominio finito, transferencias monotonas y deduplicacion de cola |
| Una especializacion aparece durante el analisis | Oleadas cerradas y siguiente conjunto ordenado |
| La optimizacion aumenta demasiado la memoria | Telemetria de pico y limites por etapa |
| Un cambio de fixture oculta una regresion | Preservar los casos y corregir la implementacion |

## 11. Fuera de alcance inmediato

No forman parte de estas seis tandas:

- cache semantica persistente entre ejecuciones;
- compilacion distribuida;
- soporte de otras plataformas;
- optimizacion de O1, O2 u O3;
- cambios de sintaxis o semantica del lenguaje;
- rediseño de LLVM o NSLink.

La cache incremental persistente sera la siguiente capa logica una vez que los resultados por callable sean
aislados, deterministas y serializables. Implementarla antes multiplicaria la complejidad y congelaria estructuras
globales que precisamente deben desaparecer.

## 12. Definicion de terminado

La reingenieria queda cerrada solo cuando se cumpla todo lo siguiente:

- las seis tandas tienen commit y documento propios;
- no queda recursion en los recorridos profundos tocados por este plan;
- `construction` no redescubre los sitios ya analizados;
- las consultas calientes definidas en la tanda 1 usan indices;
- ownership evita copias globales innecesarias;
- escape usa una worklist de dependencias;
- los callables se analizan en contextos aislados;
- `-j N` acelera `method-bodies` de forma real;
- el resultado es determinista entre diferentes conteos de trabajos;
- las pruebas dirigidas pasan en O0;
- la medicion final documenta tiempo por etapa y memoria;
- no se declara certificacion hasta que el usuario ejecute y apruebe el baseline final.

## 13. Decision tecnica

La solucion propuesta no es aumentar la pila, agregar mas hilos alrededor del modelo actual ni esconder el costo con
una cache prematura. La secuencia correcta es:

**indexar, dejar de recorrer dos veces, compactar flujo, propagar por dependencias, aislar callables y finalmente
paralelizar por oleadas deterministas.**

Esa secuencia ataca las causas medidas, reduce el riesgo de romper semantica y convierte `-j 12` en una opcion util
durante la etapa que realmente consume el tiempo de compilacion.
