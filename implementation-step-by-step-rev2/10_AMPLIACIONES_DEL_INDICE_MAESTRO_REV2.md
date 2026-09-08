# Ampliaciones del índice maestro — Revisión 2

## 1. Propósito de este documento

El índice maestro de 211 capítulos sigue siendo la columna vertebral del proyecto editorial. Esta revisión no lo sustituye ni intenta fingir que la secuencia pedagógica y la secuencia material fueron idénticas. Su función es añadir las piezas que sólo podían conocerse después de inspeccionar `A:\native-sharp-historicos`.

Las ampliaciones se expresan como interludios con sufijos —por ejemplo, 019A— para no renumerar los capítulos 000–210. Cada interludio tendrá entidad propia durante la redacción y podrá acabar como sección interna, apéndice del capítulo o capítulo breve de enlace. “Breve” no significa superficial: cada uno debe conservar evidencia, contexto, decisiones, errores, comandos, resultados y consecuencias.

## 2. Regla editorial nueva: tres tiempos distintos

Cada capítulo de la historia deberá distinguir explícitamente tres tiempos:

1. **Tiempo conceptual:** cuándo aparece una idea en especificaciones, planes o conversaciones.
2. **Tiempo material:** el primer paquete o árbol conservado donde existe una implementación observable.
3. **Tiempo narrativo:** el momento elegido para enseñar la construcción desde cero.

Estos tiempos pueden coincidir, pero no se debe presuponer. Por ejemplo, comenzar la reconstrucción con un `main.cpp` vacío es la decisión pedagógica correcta, aunque el primer árbol C++ conservado ya tenga driver, información del target, layout de datos y pruebas. La historia debe decir ambas cosas sin convertir la reconstrucción en una falsa transcripción del pasado.

## 3. Contrato de evidencia por capítulo

A partir de Rev2, todo capítulo histórico debe abrir o cerrar con una ficha como ésta:

| Campo | Contenido obligatorio |
|---|---|
| Base pedagógica | Qué se construye desde el snapshot anterior |
| Fuente histórica primaria | Ruta exacta del paquete, árbol, Markdown, manifest o worklog |
| Fuente de corroboración | Segunda evidencia independiente, cuando exista |
| Estado de evidencia | Observado, inferido, reconstruido o pendiente |
| Delta material | Archivos añadidos, modificados y retirados |
| Prueba contemporánea | Qué consta que se ejecutó entonces |
| Prueba reproducida | Qué ejecutó el autor del libro hoy |
| Diferencia conocida | Cualquier separación entre relato, paquete y estado actual |

Nunca se escribirá “las pruebas pasaron” si sólo existe una expectativa en un plan. Debe decirse “el worklog registra 29/29”, “la suite fue reproducida con 29/29” o “no existe constancia de ejecución”, según corresponda.

## 4. Interludios para los capítulos 000–018: diseño antes del código

### Interludio 000A — El archivo anterior al compilador

Presentará `NativeSharp_Parte1_snapshot_00_03` como el último checkpoint temprano observado que contiene documentos, curso, README, worklog y manifest, pero no el compilador. Debe enseñar a verificar una ausencia sin confundir “no está en este paquete” con “nunca existió en otro lugar”.

### Interludio 003A — La arquitectura escrita antes de su árbol C++

Comparará los documentos tempranos de arquitectura con el primer árbol conservado. La pregunta no será sólo qué acertó el diseño, sino qué nombres, límites o contratos cambiaron al materializarse.

### Interludio 004A — Especificación normativa contra comportamiento observado

Construirá una tabla requisito–implementación–prueba. Las especificaciones 0.1 se tratarán como intención normativa; los snapshots y pruebas decidirán qué estaba implementado en cada corte.

### Interludio 018A — El formato objetivo como restricción histórica

Separará tres niveles que en un relato comprimido pueden parecer uno solo: producir IR correcto, producir COFF estructuralmente válido y producir un PE que realmente se ejecute en Windows o Wine.

## 5. Interludios para los capítulos 019–027: arranque reconstruido

### Interludio 019A — El `main` vacío que no sobrevivió

El capítulo 019 debe declararse reconstrucción pedagógica. No se ha observado en el archivo un paquete cuyo único avance sea un `main.cpp` vacío. El primer código preservado está en `NativeSharp_snapshot_00_06` y ya representa varias decisiones acumuladas.

El texto debe reconstruir el `main` vacío porque ésa es la unidad mínima correcta para el lector, pero incluirá una caja forense con:

- la ruta del primer `main.cpp` observado;
- su contenido y responsabilidades reales;
- los componentes que ya lo acompañaban;
- la razón por la que el libro retrocede a un estado anterior no conservado;
- el procedimiento para certificar que el resultado del capítulo converge luego con Stage 06.

### Interludio 020A — Primer build reproducido frente a primer build registrado

El lector debe aprender que un build actual no prueba por sí solo qué ocurrió históricamente. Este interludio conservará ambos registros: el resultado del worklog antiguo y la reproducción moderna con versiones concretas del compilador y CMake.

### Interludio 021A — Stage 06 como primer ancla material

Describirá el árbol observado: CMake, `src/nsc/main.cpp`, Driver, TargetInfo, TargetDataLayout y prueba. Registrará el resultado histórico 4/4 tanto con GCC como con Clang. También dejará claro que la compilación de `.ns` todavía se rechazaba deliberadamente.

### Interludio 027A — Diagnóstico temprano y frontera de responsabilidad

Comparará lo que ya sabía reportar el driver con lo que más tarde pertenecería a SourceManager, Diagnostics y Lexer. Así se evita proyectar hacia atrás la arquitectura madura.

## 6. Interludios para los capítulos 028–047: el frontend 07–10

### Interludio 028A — Una entrega física, cuatro etapas didácticas

`NativeSharp_delivery_07_10` conserva una entrega conjunta, no cuatro paquetes independientes. El libro puede mantener las etapas 07, 08, 09 y 10 como progresión explicativa, pero debe identificar los límites como deltas reconstruidos dentro de una sola evidencia material.

### Interludio 035A — Cómo recuperar el orden dentro de una entrega acumulada

Enseñará el método para ordenar SourceManager, Diagnostics, Lexer, Parser, AST y Sema usando dependencias entre includes, CMake, pruebas, documentos y worklogs. Cuando dos órdenes sean compatibles con la evidencia, el texto marcará que la elección es pedagógica.

### Interludio 041A — El parser que sirve de test al lexer

Documentará no sólo la implementación de precedencia, sino las clases de defectos del lexer que se vuelven visibles al introducir expresiones estructuradas.

### Interludio 047A — Auditoría de las 116 piezas

El cierre del frontend deberá inventariar los 116 archivos observados en la entrega 07–10, enlazar los ocho documentos conservados, validar el manifest y separar evidencia directa de reconstrucción de subetapas.

## 7. Interludios para los capítulos 048–070: IR, SSA, LLVM y PE

### Interludio 048A — Por qué el archivo favorece una capa intermedia

Usará la sucesión real de paquetes 11–19 para mostrar que HIR, MIR, CFG, dominadores, SSA y pass management no fueron una digresión académica: crearon superficies de verificación antes de entrar en LLVM.

### Interludio 058A — Stage 16 no estuvo siempre verde

La narración debe conservar el corte 56/57 además del resultado final 61/61. Se explicará qué falló, cómo se diagnosticó y qué cambió. El valor histórico está precisamente en no borrar el estado intermedio fallido.

### Interludio 065A — Herramientas ausentes también forman parte del build

Stage 18 debe incluir el incidente alrededor de herramientas de inspección —por ejemplo, la ausencia de `xxd` o una comprobación por expresión regular que no representaba correctamente el artefacto— y el cambio relacionado con el data layout del target. El capítulo distinguirá defecto del compilador, defecto del test y dependencia del entorno.

### Interludio 068A — Verificar PE sin ejecutarlo

Stage 19 registra 76/76 y 10/10 comprobaciones estructurales de PE, pero no una ejecución real por falta de Windows/Wine en aquel entorno. El texto no llamará “end-to-end ejecutado” a una validación estructural.

### Interludio 070A — Stage 20 existe dentro del proyecto consolidado

No se ha observado una carpeta separada `delivery_20`. El estado se encuentra en `NativeSharp_Parte1_Proyecto_Completo`. El capítulo debe enseñar a encontrar una etapa material cuya frontera de empaquetado no coincide con su número lógico.

## 8. Interludios para los capítulos 071–086: proyecto y OOP inicial

### Interludio 071A — El cambio de Parte 1 a Parte 2

Mostrará la transición documental y material entre el capstone procedural y la especificación OOP. Debe identificar qué contratos se mantienen y cuáles se reabren.

### Interludio 071B — Stage 21 sin certificación completa contemporánea

La entrega 21 no debe recibir retrospectivamente una suite completa que no fue ejecutada en su momento por dependencias no disponibles. Se narrará qué sí se inspeccionó y cómo se puede recertificar hoy sin falsear el registro.

### Interludio 072A — Stage 22 y el segundo corte sin CTest integral

Aplicará la misma disciplina a Stage 22. El lector aprenderá que “entrega conservada” y “entrega certificada” son propiedades distintas.

### Interludio 076A — La suite vuelve a ser ejecutable

Stage 23 aporta 87/87. El texto tratará este resultado como la recuperación de una cadena de prueba completa y no como prueba automática de que todo el contenido previo pasó en su fecha original.

### Interludio 086A — La escalera cuantitativa 21–30

El cierre incluirá la progresión observada de archivos y pruebas: 461, 489, 540, 557, 591, 610, 635, 659, 681 y 715 archivos; desde 87/87 en Stage 23 hasta 106/106 en Stage 30. También registrará el worklog de Stage 29 cuya línea nombra erróneamente Stage 28.

## 9. Interludios para los capítulos 087–099: herencia, dispatch y Recovery

### Interludio 087A — Macro A como ancestro material

Introducirá `MacroA` como el checkpoint físico que agrupa Stages 31–34: 802 archivos y 116/116 pruebas registradas.

### Interludio 093A — La genealogía que contradice la numeración

Éste es uno de los capítulos históricos centrales. El orden lógico 34→35→36→37 no coincide con el orden material observado:

```text
MacroA (31–34)
    └── Stage37
          ├── Stage35 Recovery aplicado sobre Stage37
          └── Stage36 Recovery aplicado sobre Stage37 + Recovery35
                └── Stage38
```

El relato deberá explicar por qué la numeración dejó de representar ascendencia, qué se recuperó, qué se tomó como base y cómo se comprueba mediante árboles, manifests, worklogs y deltas.

### Interludio 093B — Resultado enfocado no equivale a suite global

Stage36 Recovery registra 95/95 pruebas enfocadas. No debe presentarse como el mismo universo que los 126/126 de Recovery35 o los 138/138 de Stage38.

### Interludio 099A — Volver a una línea acumulativa

Stage38, con 923 archivos y 138/138, servirá como punto donde la historia material vuelve a tener un descendiente consolidado después de los recoveries.

## 10. Interludios para los capítulos 100–113: agregados, closures, genéricos y EH

### Interludio 100A — Las descripciones CMake también envejecen

En Stage42 y Stage43 se observan descripciones que todavía nombran Stage41. El libro conservará esta discrepancia como ejemplo de metadato copiado que no tiene la misma autoridad que el contenido y las pruebas.

### Interludio 108A — Monomorfización vista como crecimiento material

Relacionará el diseño AOT con el crecimiento de snapshots 39–44: 961, 1000, 1061, 1085, 1123 y 1166 archivos. Los conteos no prueban calidad, pero ayudan a detectar pérdidas, empaquetados incompletos y cambios de escala.

### Interludio 110A — Stage 45 no es un único paquete

Reconstruirá los subcortes 45.1–45.12. Se observan 45.1–45.7 y 45.9–45.12, pero no un paquete nombrado 45.8. La ausencia se registrará como hueco nominal; no se inventará su contenido.

### Interludio 113A — Fallos, huecos y descripciones obsoletas

El cierre de Stage 45 integrará los defectos técnicos con los defectos del archivo: el salto de 45.7 a 45.9, diferencias de conteo y descripciones que no se actualizaron al mismo ritmo que el código.

## 11. Interludios para los capítulos 114–128: productos, ABI y ownership

### Interludio 114A — Cuando una etapa tiene varios cuerpos

Stages 46–48 aparecen sobre todo como ZIPs raíz con variantes `CANDIDATE`, `FULL`, `SOURCE_ONLY`, copias y entregables de handoff. El capítulo enseñará a no elegir por el nombre más atractivo, sino por manifest, hash, contenido y relación con el siguiente checkpoint.

### Interludio 117A — NSMETA como contrato histórico, no sólo formato

Debe rastrear cada versión de metadata hasta sus consumidores. Una estructura serializada no queda validada por poder escribirse; también debe poder ser leída, rechazada de forma estable cuando es incompatible y usada para reconstruir semántica.

### Interludio 118A — Candidatos, entregas completas y consumidores

Comparará los paquetes candidatos con los completos y los source-only. Documentará qué archivos de build, binarios o pruebas de consumidores cambian la interpretación de cada paquete.

### Interludio 128A — Las tres anomalías de Stage 50

El cierre de Stage 50 tendrá que incluir:

- una versión `0.1.0` después de estados que declaraban `0.2.0`;
- el paquete `FULL` 50.5 que incorpora artefactos de build;
- el `SOURCE_ONLY` 50.11 observado vacío.

Ninguna anomalía debe “arreglarse” silenciosamente en la historia. Se documentará si es regresión real, problema de empaquetado o convención de nombre insuficiente.

## 12. Interludios para los capítulos 129–140: biblioteca, entorno y concurrencia

### Interludio 129A — Stage 51 como cambio de consumidor

La biblioteca base deja de ser un apéndice y empieza a presionar al compilador. Este interludio mapeará correcciones que nacen al intentar escribir Core en el propio lenguaje.

### Interludio 132A — Stage 52: crítica, reapertura y ejecución real

La historia incluirá la revisión crítica que reabrió trabajo aparentemente terminado y la primera ejecución observada bajo Wine que detectó un defecto heredado de Stage 50. Es una demostración importante de por qué estructura válida y ejecución válida no son sinónimos.

### Interludio 133A — Dos ZIP por ronda desde Stage 53.4

Se documentará la convención expresada por el propio autor: un ZIP incremental y otro completo por ronda. La decisión responde a un fallo real de archivos intermedios ausentes en CMake sobre Windows. A partir de aquí, el tipo de paquete forma parte del significado histórico.

### Interludio 140A — El runtime como matriz de plataformas reales

Los capítulos de archivos, threads, sincronización y procesos deberán anotar dónde se probó cada comportamiento: Windows, Wine, análisis estructural o pruebas aisladas. No se extrapolará una plataforma desde otra.

## 13. Interludios para los capítulos 141–159: cierre del compilador completo

### Interludio 141A — Interop como presión sobre todas las capas

Conectará sintaxis, tipos, layout, HIR, MIR, LLVM, metadata, headers y consumidores. El objetivo es que el lector vea el cambio completo y no sólo la última función que emite código.

### Interludio 154A — Dos canales de Console y evidencia observable

Además del API, el capítulo deberá conservar ejemplos de salida, diferencias entre stdout/stderr y pruebas que hacen visible el comportamiento en lugar de limitarse a inspeccionar el binario.

### Interludio 158A — Los bugs de extremos como índice transversal

Cada bug de ownership, EH o valores extremos tendrá un identificador estable y enlaces hacia el capítulo donde nació la decisión, donde apareció el síntoma y donde quedó una prueba de regresión.

### Interludio 159A — Stage 56 y los hitos CC1/CC2

Separará el nombre del milestone, el contenido efectivo del paquete y la certificación disponible. El material tardío no debe comprimirse en “compilador completo” sin mostrar qué todavía dependía de consumidores externos.

## 14. Interludios para los capítulos 160–171: gates y ecosistema

### Interludio 160A — El archivo deja de tener un solo protagonista

NSCBuilder, NSASM, NSLink, bindings y suites de consumidores se clasificarán por función: supervisor, ensamblador, linker, consumidor ABI o gate. No se narrarán como si fueran etapas internas idénticas de `nsc`, pero sí como instrumentos que descubren defectos del compilador.

### Interludio 161A — Gate G00 como documento ejecutable

El capítulo deberá publicar precondiciones, orden, inputs, outputs y criterio exacto de fallo. Una etiqueta “G00 pasó” sin esa información es insuficiente para reconstrucción.

### Interludio 165A — Defectos hallados fuera del repositorio principal

Cada defecto reportado por un consumidor debe volver a la capa responsable y a la prueba mínima que lo reproduce. Esta trazabilidad será parte del índice de bugs.

### Interludio 170A — NSLink como autoridad independiente

Explicará qué propiedades valida el linker que el frontend o LLVM no pueden certificar por sí solos, y dónde sus diagnósticos revelan inconsistencias de ABI o metadata.

## 15. Interludios para los capítulos 172–190: compilación separada y ABI 1.0

### Interludio 172A — Frontera del archivo histórico inspeccionado

La colección raíz estudiada aporta antecedentes y paquetes tardíos, pero el estado moderno también vive en los Markdown actuales y el checkout de desarrollo. El capítulo deberá señalar la fuente concreta de cada afirmación y no atribuir al archivo de agosto hechos posteriores.

### Interludio 180A — Cerrar 0.9 requiere un consumidor limpio

No bastará con generar una biblioteca. La reconstrucción deberá limpiar el consumidor, descubrir artefactos sólo mediante contratos públicos y demostrar que no depende accidentalmente del árbol de build del productor.

### Interludio 185A — Congelar significa enumerar

El ABI que se congela debe tener lista de símbolos, layouts, calling conventions, metadata, reglas de ownership, excepciones y compatibilidad. Lo no enumerado no puede considerarse congelado.

### Interludio 190A — El baseline debe ser exportable

Además de ejecutar G00, se conservarán resultados legibles por humanos y máquinas, hashes de entradas, versiones de herramientas y golden files. El baseline tiene que poder compararse con el release candidate.

## 16. Interludios para los capítulos 191–210: camino a 1.0 y atlas final

### Interludio 191A — Futuro planificado frente a historia observada

Los capítulos 191–199 se escribirán inicialmente como plan de cierre, no como acontecimientos consumados. Cuando existan artefactos reales, cada uno cambiará de estado mediante una revisión explícita.

### Interludio 196A — Un release candidate es un objeto identificable

El RC deberá tener paquete, manifest, hashes, versión, toolchain, matriz de plataformas y lista cerrada de excepciones. Un nombre de rama o una fecha no bastan.

### Interludio 199A — Qué significa reconstruir NativeSharp 1.0.0

El lector partirá del capítulo 000 y llegará a un árbol cuyo código, pruebas, paquetes y resultados puedan compararse con el release mediante manifests normalizados. La equivalencia esperada se definirá por contenido y comportamiento, no necesariamente por timestamps o bytes de binarios no reproducibles.

### Interludio 200A — Atlas físico del archivo histórico

El atlas final incluirá también un mapa de la colección histórica: paquetes primarios, duplicados exactos, árboles extraídos, manifests, worklogs, candidatos, handoffs y artefactos de build accidentales.

### Interludio 207A — Atlas de incidentes editoriales

Además de bugs de compilador, registrará fallos de empaquetado, números omitidos, descripciones obsoletas, versiones regresivas y suites parciales. Estos incidentes son esenciales para que un futuro investigador interprete correctamente el archivo.

### Interludio 208A — Recetas desde paquetes reales

Cada receta de reconstrucción indicará un checkpoint de entrada verificable, el delta que se aplica y el checkpoint de salida. Cuando el checkpoint inicial sólo sea pedagógico, se declarará y se proporcionará una prueba de convergencia con el primer paquete conservado.

## 17. Nuevos apéndices obligatorios

El conjunto final deberá ampliar los apéndices A–J con:

- **K — Catálogo de paquetes y familias duplicadas.** Un registro por hash de contenido normalizado.
- **L — Genealogía física.** Padre material, hijo material y evidencia del vínculo.
- **M — Registro de certificaciones.** Suite, plataforma, toolchain, fecha y resultado.
- **N — Registro de anomalías del archivo.** Huecos, nombres obsoletos, paquetes vacíos y builds incluidos.
- **O — Concordancia de versiones.** Versión declarada, etapa, paquete y comportamiento efectivo.
- **P — Índice de reconstrucciones pedagógicas.** Estados creados para enseñar que no sobrevivieron como paquete histórico.
- **Q — Proveniencia de conversaciones.** Conversación, fecha, decisión extraída y artefacto corroborante.

## 18. Orden recomendado de producción Rev2

La redacción completa no debe empezar por el capítulo 000 y avanzar a ciegas. El orden de investigación recomendado es:

1. Catalogar y normalizar todos los paquetes primarios.
2. Resolver familias duplicadas y variantes de empaquetado.
3. Derivar la genealogía física y los deltas.
4. Vincular documentos, worklogs y conversaciones a cada corte.
5. Reproducir milestones seleccionados y registrar el entorno.
6. Escribir primero los capítulos de incidentes que condicionan el resto: 019A, 028A, 058A, 068A, 071B, 072A, 093A, 110A, 128A, 132A y 133A.
7. Escribir los capítulos lineales entre anclas verificadas.
8. Construir los atlas y anexos a medida que nacen las referencias, no al final.

## 19. Criterio de cierre para la planificación Rev2

La planificación estará lista para pasar a redacción cuando:

- cada capítulo 000–210 tenga al menos una fuente asignada o una marca explícita de “reconstrucción/futuro”;
- cada etapa histórica tenga padre material propuesto y nivel de confianza;
- todos los ZIPs relevantes estén clasificados como incremental, completo, candidato, handoff, copia o desconocido;
- los resultados históricos estén separados de los reproducidos;
- las anomalías enumeradas tengan destino editorial;
- las conversaciones y Markdown existentes estén catalogados;
- ningún hueco se haya rellenado con una suposición presentada como hecho.

Rev2 no pretende volver el relato menos claro. Pretende que la claridad no dependa de borrar las irregularidades que hicieron real la construcción de NativeSharp.
