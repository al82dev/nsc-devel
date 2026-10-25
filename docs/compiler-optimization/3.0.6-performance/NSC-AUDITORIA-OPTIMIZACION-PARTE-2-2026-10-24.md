# NSC — Auditoría de optimización de compilación, parte 2

**Fecha:** 2026-10-24
**Rama auditada:** `development-release-candidate`
**HEAD auditado:** `871640d9e30f66f4d0c0288c3270ebf79c438617`
**Punto de comparación:** auditoría anterior en `b822a92`
**Naturaleza:** auditoría estática; no es implementación ni certificación de rendimiento

## 1. Alcance

Esta auditoría vuelve a recorrer el pipeline actual después de los avances posteriores a `b822a92`. Su objetivo es
separar lo ya resuelto, lo parcialmente resuelto y el trabajo que todavía puede reducir la latencia de NSC al
compilar archivos `.ns`.

El alcance es el proceso de compilación. No propone modificar sintaxis, semántica, ABI, NSMETA ni algoritmos del
lenguaje. Tampoco mezcla optimización del programa generado con reducción del tiempo que tarda el compilador.

No se compiló ni ejecutó nada. No se hicieron suites, baterías, minibaselines ni baselines.

Durante la redacción, HEAD avanzó desde `597013e` mediante seis commits concurrentes enfocados en semántica de
escapes y corrección de debug/codegen. Se auditó el diferencial: no cambia las cachés, snapshots, particionado ni
flujo de reutilización evaluados en este documento.

## 2. Veredicto ejecutivo

La arquitectura actual es materialmente más rápida que la auditada en `b822a92`. Ya existen reutilización de
bundles completos, cachés de objetos LLVM, caché del runtime, caché de import libraries, snapshots semánticos de
precompilados, índices de lookup, reutilización de digests, poda indexada y enlace incremental.

La deuda principal ya no es «poner una caché» en términos generales. Es cambiar la granularidad de reutilización:

- un build idéntico puede restaurarse como bundle;
- partes de codegen y enlace pueden restaurarse por artefacto;
- bibliotecas precompiladas pueden restaurar buena parte de su modelo semántico;
- pero una edición ordinaria todavía obliga a reconstruir prácticamente todo el frontend del proyecto fuente.

La parte 2 debe concentrarse en reutilización semántica incremental correcta, no en añadir índices indiscriminados
ni en reactivar cachés históricas cuyas llaves se calculan demasiado tarde.

## 3. Reconciliación de hallazgos anteriores

| Hallazgo anterior | Estado actual | Evidencia/interpretación |
|---|---|---|
| N-01 Caché MIR desconectada | PENDIENTE, REDISEÑO | Sigue desconectada; la llave histórica requiere construir y volcar el MIR antes del lookup. |
| N-02 Cachés C1/C3 de genéricos | PENDIENTE, REDISEÑO | Permanecen desconectadas por identidad, dependencias y reescrituras incompletas. |
| N-03 Runtime reconstruido por enlace | RESUELTO | El objeto runtime se restaura/guarda en caché con identidad de entrada y herramienta. |
| N-04 Traza Debug en cada función | RESUELTO | La instrumentación de traza quedó retirada del camino normal. |
| N-05 Caché pre-LLVM anulada en Full Debug | RESUELTO | Full Debug puede reutilizar objetos; Thin sigue desactivando esas cachés. |
| N-06 SHA de fuentes repetido por unidad | RESUELTO | Los checksums se calculan y comparten antes de procesar unidades. |
| N-07 Canonicalizaciones y búsquedas repetidas | MAYORMENTE RESUELTO | Se agregaron índices y se retiraron recorridos detectados; queda medir casos concretos. |
| N-08 Copia repetida del cierre binario | RESUELTO | La preparación evita copias idénticas y reutiliza el cierre materializado. |
| N-09 `nsc build` no reenvía timing | PENDIENTE | La invocación anidada aún no ofrece una ruta explícita y estable para `--time`. |
| N-10 Poda recorre la caché completa | RESUELTO EN RUTA NORMAL | Existe índice persistente; el recorrido queda como fallback/regeneración. |
| N-11 Reutilización del frontend | PARCIAL | Snapshots ayudan a precompilados; el proyecto editable sigue siendo monolítico. |
| N-12 Lookups lineales inventariados | MAYORMENTE RESUELTO | Muchos fueron indexados; el tracking antiguo ya no representa el HEAD actual. |

Los conteos históricos del documento de 670 elementos no deben utilizarse como denominador actual. Mezclan trabajo
ya implementado con candidatos que sólo tenían sentido antes de los nuevos snapshots e índices.

## 4. Avances confirmados en código

### 4.1 Bundle de compilación

La caché de bundles ofrece un atajo de build completo. La identidad incluye el manifiesto de entradas y artefactos;
la restauración evita frontend, LLVM y enlace cuando todo coincide.

Los hashes de artefactos del bundle se calculan en paralelo. El manifiesto reutiliza digests ya verificados o
buffers disponibles cuando es posible.

Límite: es una caché exacta de build. Una entrada relevante distinta invalida el bundle completo; no conserva el
trabajo semántico de las fuentes no modificadas.

### 4.2 Caché y poda de artefactos

La caché persistente ya tiene un índice para las decisiones normales de poda. El escaneo completo permanece como
mecanismo de recuperación cuando el índice falta o no es confiable. Esto retira un costo fijo del camino común sin
sacrificar reparación.

Las identidades de herramientas de enlace participan en las llaves de archivos estáticos y enlaces incrementales,
evitando reutilizar resultados generados por herramientas incompatibles.

### 4.3 Metadata y superficies precompiladas

La búsqueda de un bundle exacto ocurre antes de materializar todas las superficies precompiladas. En un hit se
evita regenerar productos que no serán consumidos.

La metadata todavía debe cargarse y decodificarse para establecer identidad y dependencias. Los comentarios del
pipeline documentan que esa lectura es una porción mayor de la fase que la materialización textual ya evitada.

Las superficies demandadas reducen trabajo, pero también condicionan la reutilización de snapshots, como se
detalla más adelante.

### 4.4 Snapshots semánticos

El sidecar semántico v2 puede reconstruir información de precompilados sin repetir los pases que la produjeron.
La restauración incluye, según el modelo disponible:

- nombres y declaraciones;
- miembros;
- funciones y estado de namespaces;
- herencia;
- interfaces;
- despacho virtual;
- destrucción y construcción;
- layouts de objetos;
- referencias entre modelos.

Éste es uno de los avances más importantes desde la auditoría anterior: ataca trabajo del frontend, no sólo I/O o
codegen.

Límite crítico: el snapshot se rechaza cuando la superficie del módulo fue podada. Las bibliotecas para las que se
materializa sólo una superficie demandada pueden volver al camino semántico completo. La combinación
`surface pruning + semantic snapshot` todavía no está cerrada.

### 4.5 Sintaxis y genéricos

La caché de corpus de parseo de plantillas continúa desactivada. Las cachés C1/C3 de plantillas y clones genéricos
también permanecen desconectadas de forma deliberada. Sus implementaciones antiguas no preservan correctamente:

- identidad física y canónica;
- conjunto completo de dependencias;
- reescrituras del sitio solicitante;
- relación entre especialización, módulo y NSMETA;
- invalidación ante cambios semánticos.

Reactivarlas como están puede recuperar árboles ya transformados o especializaciones sin identidad válida. No son
una optimización pendiente de conectar; son diseños pendientes de sustituir.

### 4.6 Modelos, HIR y MIR

La construcción de modelos y HIR ya incorpora índices y rutas específicas para declaraciones/superficies. Esto
reduce búsquedas repetidas, especialmente en precompilados.

La caché MIR histórica sigue desconectada. Su llave se forma usando volcados del HIR y varios modelos una vez que
el trabajo costoso ya ocurrió. Incluso un hit ahorraría una parte tardía y pagaría serialización extensa. La unidad
correcta de reutilización debe ser función/callable con dependencias semánticas explícitas, no el MIR global del
proyecto.

### 4.7 LLVM y emisión de objetos

La caché pre-LLVM puede restaurar el objeto antes de lowering, optimización y emisión. Una caché secundaria de IR
pre-optimización puede ahorrar optimización y emisión, aunque para consultarla ya se pagaron frontend, MIR,
lowering y textualización/hash del módulo.

O0 tiene una ruta directa que evita ejecutar un pipeline de optimización innecesario.

En O1–O3, el número de particiones se deriva actualmente del número de workers. Cambiar `-j` puede cambiar la
partición, los módulos resultantes y la identidad de caché aunque las fuentes sean iguales. Eso reduce estabilidad
de caché entre máquinas o configuraciones de concurrencia.

El pipeline crea un `TargetMachine` por unidad de código. Antes de intentar compartirlo debe verificarse la
seguridad concurrente y medir el costo real; no se debe convertir en una optimización por intuición.

Las configuraciones de depuración optimizada pueden forzar un módulo monolítico para preservar scopes e inlining.
En esos casos se reduce el paralelismo aunque la semántica sea correcta.

### 4.8 Runtime, imports y enlace

El objeto runtime tiene caché persistente. La ruta particionada puede comprobar primero el enlace incremental y
evitar incluso solicitar el objeto runtime si el resultado final es reutilizable.

Las import libraries generadas se cachean por DLL, conjunto de símbolos e identidad del linker. Las bibliotecas
canónicas del SDK se consumen directamente; sólo imports adicionales necesitan materialización.

El enlace incremental existe en la capa interna, pero `nsc build` no ofrece todavía una configuración explícita y
completa para controlarlo. La implementación y la experiencia de usuario siguen desacopladas.

### 4.9 Vida del proceso

`ProjectCompilationLifetime` reutiliza recursos como el worker pool dentro de la vida de la compilación. No existe
un servidor de compilación persistente y aislado que sobreviva invocaciones de `nsc`.

Cada build lanzado como proceso nuevo vuelve a pagar inicialización, apertura/lectura de metadata y reconstrucción
de estado residente que no está serializado. Ésta es una oportunidad posterior a la reutilización semántica por
unidad; un daemon sin invalidación correcta sólo conservaría más estado incorrecto durante más tiempo.

## 5. Trabajo pendiente prioritario

### P0-1 — Reutilización semántica por unidad editable

Crear una unidad de snapshot para cada archivo y/o callable del proyecto fuente. Su identidad debe incluir:

- contenido normalizado de la fuente;
- opciones que afectan semántica;
- imports y aliases visibles;
- firmas físicas/canónicas consumidas;
- versiones de módulos precompilados y NSMETA;
- dependencias entre declaraciones, cuerpos y especializaciones.

El resultado debe permitir que editar un archivo invalide sólo su cierre dependiente, no todo el frontend.

### P0-2 — Snapshots compatibles con superficies demandadas

Separar la identidad del snapshot completo de la vista podada que consume el proyecto. Una solución válida puede
restaurar el modelo completo y proyectar la demanda, o publicar fragmentos autocontenidos con dependencias
explícitas. Nunca debe instalar un snapshot parcial como si fuera el modelo completo.

### P0-3 — Caché correcta de especializaciones genéricas

Sustituir C1/C3 por artefactos de especialización con identidad canónica, módulo productor, argumentos físicos,
dependencias, reescritura del sitio solicitante y validación NSMETA. Sólo entonces puede reactivarse persistencia.

### P0-4 — MIR por función/callable

Guardar MIR por callable después de fijar su identidad semántica. La llave debe derivarse de la entrada semántica
y dependencias, no de imprimir todo el HIR/MIR. Un cambio local debe invalidar el callable y sus consumidores
reales, no el módulo completo.

### P1-1 — Particiones LLVM estables

Desacoplar la identidad de partición del número de threads. Usar una asignación determinista basada en símbolos o
grupos estables y permitir que el scheduler ejecute esas particiones con cualquier cantidad de workers.

### P1-2 — Reutilización de `TargetMachine`

Medir creación/configuración por unidad. Si el costo es relevante y LLVM permite el patrón de concurrencia, crear
un pool por worker/configuración. Si no es medible, cerrar el ítem sin introducir estado compartido.

### P1-3 — Observabilidad desde `nsc build`

Modelar y reenviar explícitamente timing, trace, caché y enlace incremental. La salida normal puede permanecer
concisa; el diagnóstico detallado debe activarse bajo solicitud sin invocar manualmente acciones internas.

### P1-4 — Proceso persistente aislado

Después de cerrar invalidación granular, mantener metadata, índices, buffers inmutables y workers entre builds.
Debe existir un protocolo de reinicio por versión, target, configuración o corrupción, y fallback al proceso normal.

### P1-5 — Verificación económica de bundles calientes

Perfilar el costo de validar artefactos restaurados. Si es material, reutilizar metadatos confiables del índice y
verificar contenido completo sólo ante cambios observables, límites de confianza o auditoría solicitada.

### P2-1 — Microíndices guiados por perfil

No continuar el inventario histórico de lookups como lista automática. Añadir índices únicamente cuando un perfil
actual señale una función dominante y documentar la invalidación del índice junto con el modelo dueño.

## 6. Tandas recomendadas para la parte 2

### P2-01 — Medición focal e identidades

- Exponer timing detallado desde `nsc build`.
- Registrar hit/miss y razón de invalidación por capa.
- Medir build frío, idéntico y edición de un archivo.
- Congelar las identidades semánticas que usarán snapshots y MIR.

### P2-02 — Snapshot de unidad fuente

- Capturar declaraciones por unidad.
- Capturar dependencias entrantes y salientes.
- Restaurar unidades no modificadas.
- Invalidar el cierre mínimo.

### P2-03 — Cuerpos y MIR incrementales

- Capturar cuerpos validados por callable.
- Publicar MIR por callable.
- Restaurar callables independientes.
- Regenerar únicamente dependientes afectados.

### P2-04 — Genéricos persistentes correctos

- Definir identidad física/canónica de especialización.
- Guardar el grafo de dependencias.
- Restaurar y reescribir el uso solicitante.
- Validar contra NSMETA y módulo productor.

### P2-05 — Precompilados podados y snapshots

- Restaurar snapshot semántico aun con demanda parcial.
- Proyectar la superficie solicitada.
- Mantener identidad y referencias físicas completas.

### P2-06 — Estabilidad y reutilización de codegen

- Estabilizar particiones LLVM independientemente de `-j`.
- Medir `TargetMachine` y compartir sólo si procede.
- Conservar compatibilidad con cachés de objetos actuales.

### P2-07 — Proceso persistente

- Definir protocolo cliente/servidor local.
- Reutilizar metadata e índices inmutables.
- Aislar sesiones y cancelación.
- Reiniciar de forma segura ante incompatibilidad.

Cada tanda requiere pruebas focales propias. Las suites y el baseline final siguen siendo gates separados ejecutados
por el usuario; no forman parte de esta auditoría.

## 7. Qué no conviene hacer

- Reactivar la caché MIR global actual.
- Reactivar C1/C3 sin rediseñar identidad y restauración.
- Serializar árboles ya reescritos como si fueran salida del parser.
- Usar el número de commits o índices añadidos como medida de rendimiento.
- Añadir hashes de dumps textuales gigantes a fases calientes.
- Construir primero un daemon y resolver después la invalidación.
- Mezclar LTO con la reducción de latencia del frontend; son objetivos y perfiles distintos.
- Prometer una reducción porcentual sin medición controlada del HEAD actual.

## 8. Valor esperado por escenario

### Build idéntico

El bundle exacto y el enlace incremental ya deben dominar. El trabajo restante es reducir validación e inicio de
proceso si el perfil demuestra que son materiales.

### Edición pequeña

Es el escenario con mayor deuda y mayor oportunidad. P0-1, P0-3 y P0-4 son las piezas que evitan reconstruir el
frontend, las especializaciones y el MIR no afectados.

### Build frío

Los snapshots de precompilados, superficies demandadas, índices y cachés de runtime/imports ya reducen trabajo. La
mejora adicional vendrá de eliminar decodificación/materialización innecesaria y de paralelismo estable, pero un
build realmente frío no puede beneficiarse de artefactos que todavía no existen.

## 9. Estado de validación

HECHO en esta auditoría:

- inspección estática del código y del historial posterior a `b822a92`;
- reconciliación de hallazgos anteriores con el HEAD auditado;
- identificación de consumidores, llaves, desactivaciones y límites actuales.

NO HECHO:

- compilación del producto;
- pruebas unitarias o externas;
- medición de tiempo, memoria, tamaño o hit rate;
- certificación de equivalencia funcional;
- implementación de cualquiera de los pendientes.

## 10. Conclusión

La primera campaña resolvió costos fijos importantes y construyó una base de cachés e identidades mucho más seria.
La siguiente ganancia grande no saldrá de otra caché global ni de más lookups aislados: requiere conservar el
frontend del proyecto editable por unidades con dependencias explícitas. Ése debe ser el centro de la optimización
parte 2. Los demás pendientes son habilitadores, estabilizadores u optimizaciones posteriores guiadas por perfil.
