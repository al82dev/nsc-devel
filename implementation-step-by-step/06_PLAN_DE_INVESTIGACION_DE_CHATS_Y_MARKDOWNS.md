# 06 — Plan de investigación de chats y Markdown

## 1. Propósito

La redacción no puede empezar suponiendo que `130_evolucion_didactica...` ya resolvió toda la
historia. Ese documento es una síntesis valiosa, pero la nueva obra necesita recuperar los pasos,
adjuntos, cambios de criterio, código empaquetado y fallos intermedios que una síntesis omite por
naturaleza.

Este plan describe el trabajo de archivo previo y paralelo a la escritura.

## 2. Fase R0 — congelar el estado de investigación

Antes de reorganizar nada:

1. inventariar todos los archivos bajo `nsc-devel`, incluidos ignorados;
2. registrar tamaño, fecha y SHA-256 de cada Markdown, ZIP, manifest, log y lista changed-files;
3. separar outputs generados de documentos autorales;
4. registrar el estado Git sólo como ancla tardía;
5. no mover ni renombrar fuentes primarias;
6. escribir el catálogo dentro de esta carpeta.

Producto: `fuentes/CATALOGO_INICIAL.csv` y `fuentes/CATALOGO_INICIAL.md`.

## 3. Fase R1 — conversaciones 00–20

### Fuentes prioritarias

- `02 Formalización de NativeSharp`
- `03 Documento normativo 03`
- `04 continuacion`
- `05 Continuar NativeSharp procedural`

### Extracción requerida

- todos los turnos, en orden antiguo→nuevo;
- adjuntos 00–20;
- libros universitarios 00–10 y 11–20;
- índices generales;
- ZIP por stage y sus hashes citados;
- handover Parte 1→Parte 2;
- claims de GCC, Clang, sanitizers y rebuild desde ZIP;
- correcciones de metodología pedidas por el autor.

### Preguntas que debe resolver

- ¿Qué conversación contiene el documento `00` original?
- ¿Dónde está la especificación `01` canónica?
- ¿Los documentos 02–05 recuperados coinciden con los contratos actuales?
- ¿Qué snapshot exacto inició Stage 06?
- ¿Cómo se construyó físicamente el primer `main`?
- ¿Existen ZIP originales de cada Stage 11–20 o sólo hashes/referencias?
- ¿Qué deltas pueden extraerse automáticamente?

Producto: cronología `T00_20`, catálogo de adjuntos y tabla de gaps.

## 4. Fase R2 — conversaciones 21–30

### Fuentes prioritarias

- `06 parte 24`
- `comienza parte 25!`
- handovers Stage 20→21 y 24→25

### Extracción requerida

- entrega 21/22 que antecede al chat de Stage 23;
- ZIP y manifest de Stage 23–30;
- docs de project system, graph, namespace, class, member, body, construction y layout;
- baseline/test counts por stage;
- cambios de frontera del roadmap original.

### Discrepancias a estudiar

- stages previstos frente a stages finalmente usados;
- mensajes de error que conservan números históricos distintos;
- momento exacto en que `NativeSharp 0.1` y `nsc 0.2.0-devXX` coexistieron;
- separación real entre pipeline procedural y OOP.

Producto: cronología `T21_30` y serie reconstruible de snapshots.

## 5. Fase R3 — conversaciones 31–45

### Fuentes prioritarias

- `Plan parte 3`
- `Plan parte 4`
- `Plan Parte 5 RTTI`
- `Implementar Stage 45` si se recupera por separado

### Extracción requerida

- Macroentrega A 31–34;
- Megaentrega B y recoveries 35/36;
- Stage 37/38 y sus incidencias de entrega;
- Stage 39–44;
- Stage 45 completo, incluidas interrupciones y certificación final posterior;
- manuales/ZIP de cada stage.

### Caso forense obligatorio

Stages 35 y 36 deben tener una línea temporal doble:

```text
plan/intención
  → claim de implementación
  → snapshot donde falta
  → detección
  → recovery
  → tests que fijan la recuperación
```

### Otro caso obligatorio

Stage 38 debe mostrar la diferencia entre trabajo en árbol y entregable: un árbol que compila no se
consideró cerrado hasta corregir `DriverTests`, ejecutar matrices y reconstruir desde ZIP.

Producto: cronología `T31_45`, incident reports y matriz de features por snapshot.

## 6. Fase R4 — conversaciones 46–52

### Fuentes prioritarias

- `partiendo de release 0.1`
- `Corrección de versionado`
- `Corrección de versionado continuacion 1`

### Extracción requerida

- stages 46–48.6 y manual de DLL;
- Stage 49 completo;
- plan y checkpoints 50.1–50.10;
- documentos/ZIP de String Stage 51;
- colecciones Stage 52, `foreach`, revisiones técnicas y reportes Windows;
- relación entre versiones incorrectas/corregidas del ejecutable.

### Casos forenses obligatorios

- ownership source de `foreach` y double free;
- report inicial 206/210 frente a report posterior 210/210;
- distinguir regresión real de problema de entorno/comando de enlace;
- versión `nsc 0.1.0-dev50` citada durante checkpoints frente a identidad posterior.

Producto: cronología `T46_52` y mapa de ownership desde parser a NSMETA.

## 7. Fase R5 — Markdown 51–120 y stages 53–57

Se leerán completos, por familias:

1. String y colecciones;
2. time/environment/filesystem/paths;
3. threads/atomics/mutex/monitor/semaphore/event;
4. process/hardware/diagnostics;
5. interop C1–C8;
6. language completion;
7. CC1/CC2;
8. bloques 0–5 y self-hosting groundwork;
9. bugs encontrados;
10. listas `*_CHANGED_FILES.txt` y logs correspondientes.

Por cada documento se extraerá:

- problema inicial;
- funciones/archivos tocados;
- decisiones;
- tests;
- bugs encontrados;
- resultados;
- deuda;
- referencias a chats o reportes externos.

Producto: matriz `DOC51_120` y borradores de capítulos 129–159.

## 8. Fase R6 — versiones 0.7.x y 0.8

### Fuentes documentales

- `121_next_steps_for_the_1_0_glory.md`
- `122_native_sharp_0_7_6_alpha_big_delivery.md`
- `123_plan_implementacion_0_8_alpha.md`
- `124_gate1_nsc_limit_005.md`
- `125_nsc_limit_011_constraints_cross_project.md`
- `126_native_sharp_0_8_0_alpha_closure.md`
- `_nscbuilder/history/*`

### Método

El plan 0.8 se comparará gate por gate con reports históricos. Cada gate recibirá uno de estos
estados:

```text
PLANNED
STARTED
IMPLEMENTED
LOCALLY_VERIFIED
LANDED
CERTIFIED
DEFERRED
SUPERSEDED
```

No se afirmará que el documento de plan fue ejecutado completo por el solo hecho de existir un
cierre 0.8.

Producto: capítulos 160–171 respaldados por runs específicos.

## 9. Fase R7 — 0.9 y E1–E4

### Fuentes documentales

- `127_native_sharp_0_9_0_alpha_libraries.md`
- `128_regla_de_generacion_por_pasada.md`
- `129_cierre_native_sharp_0_9_0.md`
- `130_evolucion_didactica_nativesharp_hasta_0_9_0.md`
- `131_bitacora_por_cambio_independiente_de_git.md`
- `132_1_0_e1_objetos_precompilados_completos.md`
- `133_1_0_e2_genericos_entre_bibliotecas.md`
- `134_1_0_e3_lifetime_delegates_excepciones_entre_modulos.md`
- `135_1_0_e4_congelacion_abi_y_nsmeta_1_8.md`
- `136_baseline_certificado_1_0_e1_e4.md`

### Verificación

- reproducir o inspeccionar G00 `20260907-112232-d023cfe0`;
- comprobar los hashes documentados;
- identificar qué archivos formaron los 1013 bajo huella;
- mapear cada cambio E1–E4 a tests y modelos;
- verificar la diferencia entre 0.9.0, ABI 1.0 y NSMETA 1.8.

Producto: capítulos 172–190 y acta de evidencia.

## 10. Fase R8 — tramo futuro 1.0 RC→1.0.0

Esta fase será una bitácora viva. Por cada cambio futuro se capturará **durante la implementación**:

- problema observable;
- decisión;
- archivos y funciones;
- test rojo/verde;
- resultados de gates;
- compatibilidad ABI;
- artefacto instalado;
- efecto sobre release notes;
- deuda restante.

No se esperará al final para reconstruirlo desde Git.

## 11. Fase R9 — entrevistas y huecos del autor

Cuando las fuentes no resuelvan intención personal, se prepararán preguntas concretas, no una
petición vaga de “cuéntame la historia”. Ejemplos:

- ¿Cuál fue el primer programa que motivó NativeSharp?
- ¿Qué experiencia concreta con C#/CLR o C++ hizo deseable el lenguaje?
- ¿Cuándo se eligió el nombre?
- ¿Cuál fue el primer archivo real y en qué entorno se compiló?
- ¿En qué momento apareció la ambición educativa?
- ¿Qué stage se sintió como el primer compilador “real”?
- ¿Qué bug cambió más la arquitectura?
- ¿Qué definición personal de 1.0 debe preservarse?

Las respuestas se marcarán como testimonio del autor y se cruzarán con artefactos, sin reducir su
valor humano a una prueba mecánica.

## 12. Criterio de cierre de investigación

Un tramo estará listo para redactarse cuando:

- todos los chats accesibles se hayan leído completos;
- todos los adjuntos recuperables tengan hash;
- todos los Markdown del tramo estén catalogados;
- exista un mapa de snapshots;
- cada claim de certificación tenga log o etiqueta de confianza;
- las contradicciones estén resueltas o explicitadas;
- el checkpoint anterior y posterior puedan construirse o reconstruirse;
- haya una lista de preguntas que sólo el autor puede contestar.

