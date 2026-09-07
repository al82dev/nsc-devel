# 01 — Fuentes, evidencia y verdad histórica

## 1. Por qué hace falta una metodología forense

Git no cubre toda la vida de NativeSharp. Fue incorporado con utilidad real en las últimas etapas,
pero la creación temprana vivió principalmente en conversaciones, documentos generados, ZIP de
entregas, handovers, manifests, worklogs y snapshots. Usar `git log` como relato principal
eliminaría precisamente el periodo que esta obra debe reconstruir.

Por otra parte, una conversación puede afirmar “Stage X terminado” aunque el ZIP no se haya creado,
un documento puede describir una feature que se perdió en el siguiente snapshot y un test verde
puede haber ejecutado un `nsc.exe` distinto del que se pretendía medir. La historia requiere una
jerarquía explícita de evidencia.

## 2. Clases de fuentes

### 2.1 Conversaciones

Se usarán para recuperar:

- motivación y decisiones originales;
- orden real de solicitudes;
- alternativas discutidas;
- correcciones de rumbo exigidas por el autor;
- nombres y hashes de ZIP entregados;
- claims de tests y toolchains;
- límites declarados de cada stage;
- adjuntos que deben volver a localizarse.

Las conversaciones no se ejecutan. Por tanto, sus afirmaciones técnicas necesitan corroboración.

### 2.2 Markdown contemporáneo a cada entrega

Incluye especificaciones, implementación paso a paso, matrices, worklogs, package status,
handover, auditorías y libros. Conservan detalle que un chat resumió o que el código final ya no
hace evidente.

Los Markdown serán leídos completos. Los títulos y primeras secciones no bastan para reconstruir
una etapa.

### 2.3 ZIP y snapshots congelados

Son la fuente más valiosa para responder “qué existía físicamente al cerrar esta etapa”. Se
registrarán:

- nombre canónico;
- SHA-256 citado;
- SHA-256 recalculado si el archivo se recupera;
- manifest interno;
- número de archivos;
- identidad reportada por `nsc --version`;
- resultado de extracción limpia;
- resultado de build y tests desde la extracción;
- delta frente al snapshot anterior.

### 2.4 Código actual

El árbol actual demuestra la forma acumulada final y permite ubicar autoridades vigentes:
`ProjectMethodBodyModel`, `ProjectGenericModel`, `LibraryMetadata`, `ProjectOopLlvmLowerer`, etc.
No demuestra por sí solo en qué orden aparecieron ni qué intentos se descartaron.

### 2.5 Pruebas, fixtures y outputs

Son evidencia ejecutable. Debe distinguirse entre:

- unit test de una estructura;
- verifier de una representación;
- comparación diferencial;
- test de ejecución real;
- inspección estructural COFF/PE;
- conformance ABI contra un productor independiente;
- test omitido por falta de Windows/Wine;
- log histórico que ya no corresponde al árbol actual.

### 2.6 Manifests y listas `*_CHANGED_FILES.txt`

Permiten reconstruir deltas cuando Git no existe o no es útil. Las listas de archivos cambiados
serán tratadas como índices, pero cada cambio citado deberá comprobarse contra un snapshot o contra
el código acumulado cuando sea posible.

### 2.7 Git

Se usará para:

- el tramo donde sí hubo commits contemporáneos;
- confirmar fechas y deltas tardíos;
- detectar cambios no documentados;
- preservar la relación con el estado actual.

No se usará para inventar una historia anterior a la genealogía que conserva.

## 3. Jerarquía de confianza

Cada afirmación histórica recibirá una etiqueta:

| Etiqueta | Significado | Ejemplo |
|---|---|---|
| `CERTIFICADO` | snapshot recuperado, manifest válido, build limpio y gate reproducido | ZIP Stage 27 recompilado desde extracción |
| `OBSERVADO` | existe en el código o artefacto actual y puede inspeccionarse | `ProjectGenericModel` actual |
| `CORROBORADO` | dos o más fuentes independientes coinciden, pero falta el artefacto original | chat + Markdown + test actual |
| `DOCUMENTADO` | una fuente contemporánea lo afirma con detalle | worklog de la entrega |
| `DECLARADO` | aparece como claim en una conversación | “176/176 PASS” sin log recuperado |
| `RECONSTRUIDO` | inferencia técnica necesaria a partir del estado anterior y posterior | delta temprano sin ZIP |
| `PLANEADO` | decisión o trabajo futuro, todavía no producto | estabilización de 1.0 RC |
| `DESCONOCIDO` | falta evidencia suficiente | contenido de un adjunto no recuperado |

La prosa deberá conservar la etiqueta cuando pueda cambiar la interpretación. “Stage 35
implementó…” no equivale a “un chat declaró Stage 35 terminado”.

## 4. Regla de resolución de contradicciones

Cuando dos fuentes discrepen:

1. no se elegirá silenciosamente la versión más reciente;
2. se registrarán ambas afirmaciones;
3. se compararán snapshots, código, manifests y tests;
4. se determinará si una fuente describe intención, implementación parcial o producto certificado;
5. se escribirá la transición que resuelve la contradicción;
6. si no puede resolverse, quedará un bloque `INCERTIDUMBRE ABIERTA` y una tarea de recuperación.

Ejemplo de patrón:

```text
Chat A: Stage 36 terminado                 [DECLARADO]
Snapshot usado por Stage 37: feature falta [OBSERVADO]
Auditoría posterior: discordancia          [CORROBORADO]
Recovery tests + código actual             [OBSERVADO]
Conclusión: el relato existió antes que el cambio físico; se recuperó después.
```

## 5. Inventario inicial del disco

Al 7 de septiembre de 2026, el levantamiento inicial encontró:

- 170 archivos Markdown editoriales visibles mediante el inventario ordinario;
- 570 Markdown contando reportes generados e ignorados;
- 145 Markdown bajo `mkdown/`;
- 8 bajo `claude/`;
- 6 bajo `Claude outputs/`;
- 5 libros bajo `book/`;
- aproximadamente 400 reportes Markdown bajo `_nscbuilder/history/`;
- un `nsc-devel.zip` en la raíz;
- documentos `130` a `136` que ya reconstruyen la evolución hasta 0.9 y E1–E4;
- listas de archivos cambiados para stages y bloques de maduración;
- logs de dos enlazadores y gates externos.

Estas cifras son un punto de partida, no un catálogo terminado. Los archivos generados pueden
duplicar contenido y los adjuntos tempranos de chats pueden no estar presentes en el checkout.

## 6. Conversaciones históricas ya localizadas

El inventario de conversaciones accesibles incluye, entre otras, las siguientes fuentes. Los
títulos se conservan porque identifican el contexto real; no se normalizarán hasta haber extraído
su contenido y adjuntos.

| Conversación | Tramo recuperable |
|---|---|
| `02 Formalización de NativeSharp` | gramática formal, tokens, EBNF y decisiones léxicas |
| `03 Documento normativo 03` | sistema de tipos, semántica y memoria prevista |
| `04 continuacion` | cambio explícito hacia implementación didáctica; stages 07–16 |
| `05 Continuar NativeSharp procedural` | cierre 17–20, libros 00–20 y handover a OOP |
| `06 parte 24` | project graph, compilation units, namespaces y lookup |
| `comienza parte 25!` | clases parciales, members, bodies, constructores, layout y Stage 30 |
| `Plan parte 3` | receivers, herencia, vtables y destrucción |
| `Plan parte 4` | properties, indexers, operators y Stage 38 |
| `Plan Parte 5 RTTI` | stages 39–45: RTTI, value types, delegates, generics y EH |
| `partiendo de release 0.1` | stages 46–50, ABI, metadata, RAII y ownership |
| `Corrección de versionado` | implementación incremental de Stage 50 |
| `Corrección de versionado continuacion 1` | Stage 52, `foreach` y revisión de snapshots |
| `Diseño de linker independiente` | nacimiento del plan de NSLink y frontera con NativeSharp |
| `Analizar compilador actual` | evaluación de madurez posterior |
| `Comparar NativeSharp Lenguajes` | posicionamiento y compatibilidad prevista |
| `Revisión C C++ C#` | contraste de semántica y superficie |
| `Actualizarse sobre NativeSharp` | estado reciente y huecos |
| `Actualizar el compilador` | cambios recientes del árbol |
| `Revisar estado de Native Sharp` | auditoría inmediatamente anterior a este plan |

La recuperación futura deberá recorrer cada conversación desde el primer turno, descargar o
localizar todos sus adjuntos, registrar hashes y evitar depender de previews truncados.

## 7. Markdown canónicos ya identificados

### 7.1 Evolución y continuidad

- `mkdown/130_evolucion_didactica_nativesharp_hasta_0_9_0.md`
- `mkdown/131_bitacora_por_cambio_independiente_de_git.md`
- `mkdown/121_next_steps_for_the_1_0_glory.md`
- `mkdown/CONTENIDO.txt`

### 7.2 Versiones recientes

- `122_native_sharp_0_7_6_alpha_big_delivery.md`
- `123_plan_implementacion_0_8_alpha.md`
- `124_gate1_nsc_limit_005.md`
- `125_nsc_limit_011_constraints_cross_project.md`
- `126_native_sharp_0_8_0_alpha_closure.md`
- `127_native_sharp_0_9_0_alpha_libraries.md`
- `128_regla_de_generacion_por_pasada.md`
- `129_cierre_native_sharp_0_9_0.md`

### 7.3 Megaentregas 1.0 observadas

- `132_1_0_e1_objetos_precompilados_completos.md`
- `133_1_0_e2_genericos_entre_bibliotecas.md`
- `134_1_0_e3_lifetime_delegates_excepciones_entre_modulos.md`
- `135_1_0_e4_congelacion_abi_y_nsmeta_1_8.md`
- `136_baseline_certificado_1_0_e1_e4.md`

### 7.4 Maduración y correctness

Los documentos `51` a `120`, los milestones CC1/CC2, los handovers, matrices y reportes de bugs
serán catalogados por tema, no sólo por número. Los números se reutilizaron y no bastan como
identidad histórica.

## 8. Ficha obligatoria de fuente

Cada fuente incorporada al archivo histórico tendrá:

```yaml
id: SRC-XXXX
tipo: chat | markdown | zip | codigo | test | log | manifest | git
titulo: "..."
fecha_creacion: "..."
fecha_modificacion: "..."
ubicacion: "..."
sha256: "... o no disponible"
tramo: "stages/versiones cubiertas"
afirmaciones_clave:
  - "..."
corroborada_por:
  - SRC-YYYY
contradicciones:
  - SRC-ZZZZ
nivel_confianza: CERTIFICADO | OBSERVADO | ...
pendientes:
  - "..."
```

El catálogo no será un adorno bibliográfico. Cada capítulo citará los IDs que sustentan su estado
de entrada, sus cambios y su certificación.

## 9. Prohibiciones de investigación

- No citar un preview truncado como sustituto del chat completo.
- No asumir que el archivo con fecha más reciente es más verdadero.
- No copiar métricas de tests sin identificar el binario y el directorio de build.
- No tratar un skip como pass.
- No reconstruir un ZIP desde el árbol actual y presentarlo como el original.
- No atribuir a un stage una corrección posterior sin señalar el anacronismo.
- No ocultar que una fuente falta.
- No convertir una recomendación futura en intención original.

## 10. Producto de la investigación

Antes de redactar cada volumen se crearán tres artefactos:

1. un catálogo de fuentes del tramo;
2. una cronología de afirmaciones y artefactos;
3. una tabla de discrepancias resueltas y abiertas.

Sólo entonces se escribirá la narración paso a paso. Esta secuencia evita una historia elegante
pero falsa.

