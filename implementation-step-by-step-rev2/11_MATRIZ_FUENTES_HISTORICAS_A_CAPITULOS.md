# Matriz de fuentes históricas a capítulos — Revisión 2

## 1. Para qué existe esta matriz

Este documento evita que los capítulos se escriban por memoria, por semejanza con el código actual o por confianza excesiva en un único resumen. Asigna a cada bloque del índice maestro una familia de fuentes concretas y declara qué puede probar cada una.

La raíz histórica usada es:

```text
A:\native-sharp-historicos
```

La raíz se considera sólo lectura. Las rutas de esta matriz son localizadores de investigación; los capítulos publicados deberán citar además el archivo interno, sección, manifest o línea relevante.

## 2. Significado de las marcas

| Marca | Significado |
|---|---|
| **D** | Evidencia directa del estado material: código, test, manifest o binario del checkpoint. |
| **N** | Evidencia narrativa contemporánea: worklog, handover, README o documento de cierre. |
| **S** | Especificación o plan; demuestra intención, no implementación. |
| **C** | Corroboración posterior; útil, pero no reemplaza el artefacto contemporáneo. |
| **R** | Reconstrucción pedagógica necesaria porque no se conserva el corte exacto. |
| **F** | Trabajo futuro: todavía no debe narrarse como hecho consumado. |

## 3. Matriz principal 000–210

| Capítulos | Materia | Fuente histórica principal | Tipo | Uso y cautela |
|---:|---|---|:---:|---|
| 000–003 | Motivación, alcance y arquitectura | `md\00_vision_y_alcance.md`, `md\01_arquitectura_del_compilador.md`, `NativeSharp_Parte1_snapshot_00_03` | S/N/D | El snapshot prueba la existencia de la documentación y su empaquetado, no todavía la del compilador. |
| 004–018 | Lenguaje procedural, tipos, ABI y formato objetivo | `md\02_especificacion_del_lenguaje_0_1.md`, `md\03_plan_de_construccion_paso_a_paso.md`, documentos dentro de `NativeSharp_Parte1_snapshot_00_03` | S/N | Comparar después con código y tests; no convertir requisitos en logros. |
| 019–020 | Árbol vacío, primer build y warnings | `NativeSharp_snapshot_00_06` como primer ancla; corte vacío recreado por el libro | D/R | No existe un paquete observado con sólo `main.cpp` vacío. La secuencia deberá converger byte o semánticamente hacia Stage 06. |
| 021–027 | Driver, lectura de fuentes, locations y diagnóstico inicial | `NativeSharp_snapshot_00_06` y su worklog/manifest | D/N | El paquete ya agrupa varias decisiones. Separarlas en capítulos es una descomposición pedagógica. |
| 028–047 | SourceManager avanzado, lexer, parser, AST y semántica | `NativeSharp_delivery_07_10`, `NativeSharp_delivery_07_10.zip`, `NativeSharp_07_10_guia_didactica` | D/N/C | Es una sola entrega física de 116 archivos. Los límites 07/08/09/10 se reconstruyen desde dependencias, docs y pruebas. |
| 048–051 | HIR y verificador | `NativeSharp_delivery_11` | D/N | Manifest y worklog registran 29/29 en GCC, Clang y sanitizers. Reproducir aparte. |
| 052–053 | MIR y dumps | `NativeSharp_delivery_12` | D/N | El registro de baseline es 36/36. Revisar delta real contra Stage 11. |
| 054–055 | CFG y verificadores | `NativeSharp_delivery_13` | D/N | Resultado histórico 43/43. |
| 056–057 | Dominadores y frontier | `NativeSharp_delivery_14` | D/N | Resultado histórico 48/48. |
| 058–059 | Promoción a SSA y verificación | `NativeSharp_delivery_15`, `NativeSharp_delivery_16` | D/N | Stage 15 registra 53/53. Stage 16 conserva una transición fallida 56/57 y cierre 61/61; narrar las dos. |
| 060–061 | Managers y optimizaciones | `NativeSharp_delivery_17` | D/N | Resultado histórico 68/68. |
| 062–065 | LLVM C API, lowering y COFF | `NativeSharp_delivery_18` | D/N | Registra 70/70 y 11/11 en COFF/reensamblado; incluir problemas de herramientas y data layout. |
| 066–069 | Runtime, linking, PE y O0–O3 | `NativeSharp_delivery_19`, `NativeSharp_Parte1_Proyecto_Completo` | D/N | Stage 19: 76/76 y 10/10 estructural; no hubo ejecución Windows/Wine contemporánea. |
| 070 | Capstone Stage 20 | `NativeSharp_Parte1_Proyecto_Completo`, `NativeSharp_Libro_Universitario_Capitulos_00_20` | D/N/C | Stage 20 vive dentro del proyecto consolidado; no se observó `delivery_20` independiente. |
| 071 | Especificación OOP | documentos y handovers de Parte 2; `NativeSharp_Entrega21_Object_Model` | S/N/D | Distinguir el modelo especificado del implementado. |
| 072 | Sistema de proyecto | `NativeSharp_Entrega22_Project_System` | D/N | No consta CMake/CTest completo contemporáneo por dependencias ausentes. |
| 073–075 | Grafo y unidades de compilación | `NativeSharp_Entrega23_Project_Graph_Compilation_Units` | D/N | Primer retorno observado a suite integral: 87/87. |
| 076–077 | Namespaces, using y lookup | `NativeSharp_Entrega24_Namespaces_Using_Name_Lookup` | D/N | 89/89; manifest 556/556 según registro histórico. |
| 078 | Class y partial | `NativeSharp_Entrega25_Class_Partial_Class_Aggregation` | D/N | 92/92. |
| 079 | Espacios de miembros | `NativeSharp_Entrega26_Class_Member_Declaration_Spaces` | D/N | 95/95. |
| 080–081 | Scopes de instancia y overloads | `NativeSharp_Entrega27_Instance_Method_Scopes_Overload_Resolution` | D/N | 98/98. |
| 082 | Constructores | `NativeSharp_Entrega28_Constructors_Object_Construction` | D/N | 101/101. |
| 083 | Layout de objetos | `NativeSharp_Entrega29_Object_Layout_Allocation_Contract` | D/N | 103/103; una línea del worklog llama a la entrega “Stage 28”. |
| 084–086 | OOP HIR/MIR/LLVM | `NativeSharp_Entrega30_OOP_HIR_MIR_LLVM_Capstone` | D/N | 106/106. Cierre material de esta secuencia. |
| 087–092 | Receivers, herencia, vtables e interfaces | `NativeSharp_Parte3_MacroA_Stages31_34`, `NativeSharp_Parte3_Stage37_Static_Initialization` | D/N | Macro A agrupa 31–34; Stage 37 es ancestro material de recoveries numerados 35/36. |
| 093 | Incidente Recovery | `NativeSharp_Parte3_Stage35_Recovery_on_Stage37`, `NativeSharp_Parte3_Stage36_Recovery_on_Stage37plus35`, handover de Stage 37 | D/N | Fuente crítica para separar número lógico de genealogía física. Stage36 reporta 95/95 enfocado, no suite global equivalente. |
| 094–098 | Statics, const/readonly, properties, indexers y operators | Stage37, recoveries 35/36 y `NativeSharp_Parte3_Stage38_Properties_Indexers_Operators` | D/N | El reparto exacto exige deltas; Stage38 consolida 923 archivos y 138/138. |
| 099 | RTTI y casts | `NativeSharp_Parte3_Stage39_Native_RTTI_Checked_Casts` | D/N | 961 archivos observados. |
| 100–101 | Struct y ABI de agregados | `NativeSharp_Parte3_Stage40_Struct_Callable_Value_ABI`, `NativeSharp_Stage40_2_Win64_Aggregate_ABI_full.zip` | D/N | La extensión 40.2 debe compararse con el paquete base. |
| 102–103 | Arrays, strings y Core inicial | `NativeSharp_Parte3_Stage41_Arrays_Strings_NativeSharp_Core` | D/N | 1061 archivos observados. |
| 104–105 | Delegates y events | `NativeSharp_Parte3_Stage42_Delegates_Events` | D/N | La descripción de CMake puede seguir nombrando Stage41; el contenido manda. |
| 106–107 | Lambdas y closures | `NativeSharp_Parte3_Stage43_Lambdas_Closures` | D/N | Otra descripción potencialmente obsoleta; confirmar por delta y tests. |
| 108–109 | Genéricos y monomorfización | `NativeSharp_Parte3_Stage44_Generics_Monomorphization` | D/N | 1166 archivos observados. |
| 110–113 | Excepciones y EH Win64 | `NativeSharp_Parte3_Stage45_1_*` a `NativeSharp_Parte3_Stage45_12_*`, final certified y handovers | D/N | No se observó paquete nombrado 45.8. No inventar esa entrega; reconstruir el salto mediante deltas. |
| 114 | Productos de biblioteca | `NativeSharp_Parte4_Stage46_Native_Library_Products.zip`, `NativeSharp_Stage46_CHANGED_FILES_ONLY.zip` | D/N | Hay full e incremental/cambios; identificar base requerida. |
| 115–118 | ABI C, headers, NSMETA y consumidores | familias `NativeSharp_Stage47_*` y `NativeSharp_Stage48_*` | D/N | Distinguir `CANDIDATE`, `FULL`, `SOURCE_ONLY` y duplicados. Stage48.1 tiene candidato y full. |
| 119–127 | RAII, move, owned/borrowed y lowering | `NativeSharp_Stage49_*`, `NativeSharp_Handover_Stage49_to_Stage50`, `NativeSharp_Stage50_*` | D/N/S | Stage49 conserva descripción vieja de 48.3. Stage50 está fragmentado en once checkpoints. |
| 128 | Certificación Stage 50 | `NativeSharp_Stage50_FULL`, checkpoints 50.1–50.11, handovers y `00_estado_stage50.md` | D/N/C | Registrar versión 0.1.0 tras 0.2.0, build incluido en FULL 50.5 y SOURCE_ONLY 50.11 vacío. |
| 129–131 | String, colecciones, default/var | `NativeSharp_Stage51_*` y documentos `51_nativesharp_complete_string*` | D/N/S/C | Hay varias redacciones y paquetes; ordenar por contenido y timestamp, no por sufijo `(1)`. |
| 132–133 | Foreach y revisión | `NativeSharp_Stage52*`, `NativeSharp_Stage52x_Foreach_SOURCE_ONLY*`, documentos `STAGE52*` | D/N/C | Conservar crítica, reapertura, ejecución bajo Wine y deltas de revisión técnica. |
| 134–135 | Tiempo, entorno, filesystem y args | `Stage53_1_Time.zip` a `Stage53_5_CommandLineArgs.zip` | D/N | Desde 53.4 se documenta la convención incremental + completo por ronda. |
| 136–139 | Threads, atomics y sincronización | paquetes Stage54 conservados, entre ellos `NativeSharp_Stage54_3_Mutex.zip` | D/N | Completar catálogo interno antes de asignar un subcapítulo por ZIP. |
| 140 | Process, hardware y diagnostics | `NativeSharp_Stage55_Process_Hardware_Diagnostics.zip` | D/N | Identificar pruebas dependientes de plataforma. |
| 141–159 | Interop ampliado, lenguaje y compiler correctness | `NativeSharp_Stage55x_Interop_Foundation_full*`, `NativeSharp_Stage56_Compiler_Roadmap_C1_C5`, `NativeSharp_Compilador_Completo_Stage56_*`, milestone CC1 | D/N/S/C | Familia extensa con copias y extensiones C1–C8. Exige catálogo por hashes antes de afirmar orden total. |
| 160–171 | Gates, builder, 0.7.x y 0.8 | `nativesharp-compiler`, paquetes del compilador completo, NSLink, bindings, roadmaps y Markdown tardíos | D/N/S/C | Son varios productos. Usarlos como consumidores o autoridades externas, no mezclarlos en un único árbol. |
| 172–180 | Compilación separada y 0.9 | Markdown actuales del checkout `A:\nsc-devel\mkdown`, código actual y precursores NSMETA/consumidores del archivo | D/N/C | Parte del relato está fuera del horizonte temprano del archivo. Fechar cada fuente. |
| 181–190 | E1–E4, ABI 1.0 y baseline | Markdown actuales, código actual, golden tests y paquetes que se produzcan | D/N/F | No atribuir al archivo antiguo estados modernos que no contiene. |
| 191–199 | RC y 1.0.0 | plan actual; futuros candidatos, manifests y gates | F | Debe permanecer en futuro hasta que exista evidencia material certificada. |
| 200–210 | Atlas e índices | Todos los catálogos, código final, historial de conversaciones y corpus de pruebas | C/D/F | Se construyen durante toda la obra; no esperar al final para capturar referencias. |

## 4. Fuentes transversales

### 4.1 Documento histórico previo

`NativeSharp_119_Archivo_Historico_Completo_Como_Se_Construyo.md` es una narración panorámica útil y una fuente de hipótesis. Declara explícitamente que no abrió los ZIPs uno por uno, por lo que Rev2 no lo usará como autoridad final para contenido interno, duplicados o genealogía.

### 4.2 Método de trabajo

`NativeSharp_05_metodo_de_trabajo_y_archivo_historico.md` aporta convenciones de preservación, entrega y lenguaje. Entre ellas está la separación entre documentación/conversación en español y código/comentarios en inglés desde Stage52, así como la importancia de conservar el archivo histórico.

### 4.3 Handovers

Los handovers pueden explicar la intención de base y siguiente paso, pero se verificarán contra:

- archivos presentes en el paquete de origen;
- archivos presentes en el paquete de destino;
- manifest o lista de changed files;
- pruebas registradas;
- hashes del contenido común.

Un handover que dice “continuar desde X” es evidencia narrativa fuerte, no una prueba criptográfica de ascendencia.

### 4.4 Manifests

Los `MANIFEST_SHA256.txt` son fuentes directas cuando:

- su sintaxis puede parsearse sin ambigüedad;
- sus rutas se normalizan de manera reproducible;
- los hashes coinciden con los archivos del mismo paquete;
- se registra si el manifest se incluye a sí mismo o lo excluye;
- se sabe si cubre fuentes solamente o el árbol completo.

### 4.5 Worklogs

Un worklog prueba que alguien registró una acción y su resultado. No sustituye la reproducción. La obra conservará el texto original, extraerá comando, toolchain, plataforma y conteo, y añadirá una ejecución moderna como registro separado.

### 4.6 Conversaciones

Las conversaciones deben catalogarse con:

- identificador estable o título;
- fecha y zona horaria cuando estén disponibles;
- participantes;
- decisión o requisito extraído;
- archivos o paquetes mencionados;
- corroboración material;
- dudas no resueltas.

Una afirmación nacida en chat se marcará como decisión o intención hasta encontrar código, test, manifest o entrega que la materialice.

## 5. Matriz de anomalías y destino editorial

| Anomalía observada | Fuente | Capítulo/interludio de destino | Tratamiento |
|---|---|---|---|
| No se conserva el `main` vacío | Primer código en `NativeSharp_snapshot_00_06` | 019/019A | Reconstrucción pedagógica declarada. |
| 07–10 es una entrega conjunta | `NativeSharp_delivery_07_10` | 028A, 035A, 047A | Derivar subetapas sin fingir paquetes inexistentes. |
| Stage16 tuvo 56/57 antes de 61/61 | worklogs de delivery 16 | 058A | Narrar fallo y corrección. |
| Stage18 depende de herramientas del entorno | worklogs/tests de delivery 18 | 065A | Separar fallo de herramienta, test y compilador. |
| Stage19 no ejecutó el PE | worklog de delivery 19 | 068A | Llamarlo validación estructural. |
| Stage20 carece de carpeta separada | proyecto completo Parte 1 | 070A | Usar el consolidado como frontera material. |
| Stage21/22 sin CTest integral contemporáneo | worklogs respectivos | 071B/072A | No asignarles retrospectivamente una certificación. |
| Worklog de Stage29 dice Stage28 | Entrega29 | 086A | Conservar error y resolver identidad por paquete/contenido. |
| Genealogía 35/36 basada en 37 | nombres, árboles y handovers | 093A/093B | Separar orden lógico y físico. |
| CMake Stage42/43 aún nombra Stage41 | árboles respectivos | 100A/113A | Tratar descripción como metadato obsoleto. |
| No aparece Stage45.8 nominal | familia Stage45 | 110A/113A | Hueco explícito, sin contenido inventado. |
| Stage49 describe 48.3 | árbol Stage49 | 114A/128A | Confirmar identidad por delta y manifest. |
| Stage50 vuelve a VERSION 0.1.0 | paquetes Stage50 | 128A | Resolver si es regresión, copia o error de release. |
| FULL 50.5 incluye builds | checkpoint 50.5 | 128A | Registrar contaminación; no borrarla del archivo. |
| SOURCE_ONLY 50.11 está vacío | checkpoint 50.11 | 128A | Marcar entrega vacía, buscar full/handover correspondiente. |
| Primera ejecución Wine revela defecto anterior | Stage52 | 132A | Mostrar diferencia entre inspección y ejecución. |
| Dos ZIP por ronda desde 53.4 | documento/paquetes Stage53 | 133A/134–135 | Identificar incremental y completo junto con su base. |

## 6. Preguntas de investigación pendientes por bloque

### 000–020

- ¿Existe en conversaciones un mensaje con el contenido exacto del primer `main.cpp`?
- ¿Puede reconstruirse el primer CMake a partir de instrucciones previas a Stage 06?
- ¿Qué timestamp pertenece al archivo original y cuál a una copia o extracción posterior?

### 021–047

- ¿Los ocho documentos de 07–10 describen commits o cortes internos recuperables?
- ¿Qué pruebas se añadieron específicamente con lexer, parser y Sema?
- ¿Hay diffs o fragmentos en chats que permitan fijar el orden exacto?

### 048–070

- ¿Cuál fue la causa exacta de 56/57 en Stage16?
- ¿Qué comando produjo 11/11 en la comprobación COFF?
- ¿Qué parte de Stage20 cambia respecto de Stage19 y cuál sólo consolida documentación?

### 071–099

- ¿Qué comprobaciones parciales sí se ejecutaron en Stages21/22?
- ¿Puede derivarse un manifest de contenido para ambos y compararlo con Stage23?
- ¿Qué archivos exactos constituyen Recovery35 y Recovery36 sobre Stage37?

### 100–128

- ¿Hubo un Stage45.8 sin ese nombre o fue una numeración deliberadamente saltada?
- ¿Cuáles ZIPs con sufijo `(1)` son duplicados exactos y cuáles revisiones?
- ¿Dónde se originó la regresión de VERSION de Stage50?

### 129–159

- ¿Cuál es el orden por contenido de las múltiples variantes de Stage51 y Stage52?
- ¿Qué paquetes Stage53–55 son incrementales y qué base requieren?
- ¿Qué significan exactamente C1–C8 dentro de las familias Stage56?

### 160–210

- ¿Qué defectos de `nsc` se descubrieron primero en NSLink, NSASM, nsbind o bindings?
- ¿Qué decisiones sólo aparecen en conversaciones modernas?
- ¿Qué partes 0.9/1.0 ya tienen artefacto y cuáles siguen siendo roadmap?

## 7. Regla para ampliar esta matriz

Cada nueva fila debe responder cinco preguntas:

1. ¿Qué afirmación del capítulo pretende sostener?
2. ¿La fuente es contemporánea al hecho?
3. ¿Contiene estado material o sólo lo describe?
4. ¿Existe una segunda fuente que la contradiga o corrobore?
5. ¿Qué parte sigue siendo inferencia del autor?

Si una ruta desaparece, cambia de nombre o sólo existe dentro de un ZIP, el catálogo no la eliminará. Registrará la ruta observada, el hash, la localización nueva y la relación entre ambas.
