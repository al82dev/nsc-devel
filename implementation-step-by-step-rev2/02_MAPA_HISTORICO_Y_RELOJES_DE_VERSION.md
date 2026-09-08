# 02 — Mapa histórico y relojes de versión — Revisión 2

> Rev2 conserva el mapa conceptual original y añade al final la cronología material observada en
> `A:\native-sharp-historicos`. Una fecha o un número de stage no se usan por sí solos para decidir
> la ascendencia de un paquete.

## 1. El problema de una sola línea de tiempo

NativeSharp no evolucionó con un único contador. Las fuentes muestran al menos cinco relojes:

1. documentos normativos `00–05`;
2. stages pedagógicos `06–57` y subdivisiones como `40.2`, `48.6`, `53.2b`;
3. bloques de correctness `CC1`, `CC2`, `SHR-*`, `NSC-LIMIT-*` y gates `G00..`;
4. versiones públicas del compilador, desde builds `dev` hasta `0.9.0`;
5. versiones de contratos binarios: Native ABI, Runtime ABI y NSMETA.

La obra no los aplanará en una lista ficticia. Cada capítulo mostrará los relojes aplicables.

## 2. Identidades que deben conservarse por separado

| Identidad | Qué nombra | Ejemplo observado |
|---|---|---|
| versión de producto | madurez pública del compilador | `nsc 0.9.0` |
| build identity | bytes concretos de una construcción | `0.9.0+build.<fecha>.<hash>` |
| versión de lenguaje | revisión declarada de NativeSharp | `NativeSharp 0.1` |
| stage | etapa pedagógica acumulada | `stage 57` |
| Native ABI | forma binaria pública NativeSharp | `1.0` |
| Runtime ABI | contrato del runtime | `1.0` |
| NSMETA | formato de metadata de bibliotecas | `1.8` |
| gate | procedimiento de certificación | `G00` |

Una biblioteca puede estar congelada con ABI 1.0 mientras el compilador continúa en 0.9.0. Eso no
es inconsistencia: es una estrategia para estabilizar el contrato antes de proclamar el producto.

## 3. Era A — intención y contrato, documentos 00–05

```text
00  introducción, identidad y arquitectura conceptual
01  especificación inicial del lenguaje procedural
02  estructura léxica, tokens, precedencia y EBNF
03  sistema de tipos y reglas semánticas
04  data layout, memoria y semántica de runtime
05  interoperabilidad C y ABI Windows x64
```

Esta era decide qué debe significar el lenguaje antes de escribir el frontend. Sus documentos
originales deben recuperarse desde chats/adjuntos o paquetes académicos porque no todos viven como
archivos sueltos en el checkout actual.

## 4. Era B — compilador procedural, Stages 06–20

| Stage | Hito | Estado histórico general |
|---:|---|---|
| 06 | arquitectura física C++20, targets y límites de capas | implementado |
| 07 | SourceManager y diagnostics | implementado |
| 08 | lexer determinista | implementado |
| 09 | parser y syntax AST | implementado |
| 10 | símbolos, tipos y análisis semántico | implementado |
| 11 | HIR tipado | entrega congelada histórica |
| 12 | MIR no-SSA | entrega congelada histórica |
| 13 | CFG | entrega congelada histórica |
| 14 | dominance y dominance frontier | entrega congelada histórica |
| 15 | SSA | entrega congelada histórica |
| 16 | pass manager y optimizaciones propias | entrega congelada histórica |
| 17 | LLVM lowering | implementado |
| 18 | target x64 y COFF | implementado |
| 19 | runtime, linker y PE | implementado |
| 20 | optimización LLVM O0–O3 y capstone procedural | entrega congelada histórica |

La obra debe insertar antes de Stage 06 el verdadero arranque físico: carpeta, CMake, primer
`main.cpp`, primera opción de driver, primer test y primer fixture. Aunque la numeración histórica
agrupara esos pasos dentro de Stage 06, la narración no puede comenzar con toda la arquitectura ya
creada.

## 5. Era C — proyectos y OOP inicial, Stages 21–30

```text
21  especificación del object model y OOP
22  .nsproj, descubrimiento multifichero y source sets
23  project graph y compilation units
24  namespaces, using, aliases y global lookup
25  class, partial class y agregación cross-file
26  member declaration spaces, fields, methods y this
27  method scopes, locals, shadowing, conversiones y overloads
28  constructors y new C(args)
29  object layout, offsets, padding y allocation
30  OOP HIR → MIR → LLVM → COFF
```

El roadmap original cambió de fronteras durante la implementación. Por ejemplo, constructors,
layout e inheritance no siempre conservaron el número inicialmente previsto. La autoridad de un
hito será **nombre + delta + pruebas**, no el número aislado.

## 6. Era D — OOP nativo general, Stages 31–39

| Stage | Tema central |
|---:|---|
| 31 | receivers generalizados y null trap |
| 32 | herencia simple, grafo y upcasts |
| 33 | base layout y constructor chaining |
| 34 | `virtual`/`override`, slots y ABI OOP |
| 35 | abstract/sealed y destruction model |
| 36 | interfaces y dispatch por interface |
| 37 | miembros estáticos, inicialización, const y readonly |
| 38 | properties, indexers, operators y conversiones definidas |
| 39 | RTTI, `is`, `as` y checked casts |

La genealogía 35/36 requiere un interludio histórico propio: la documentación y la presencia física
divergieron, y ambas etapas tuvieron que recuperarse. El libro conservará el orden de intención y
el orden real de recuperación.

## 7. Era E — valores y features avanzadas, Stages 40–45

```text
40    struct como valor físico y ABI de agregados
40.2  carriers directos/indirectos y sret Win64
41    arrays dinámicos, string y Core mínimo
42    delegates y events
43    lambdas y closures
44    genéricos AOT por monomorfización
45    excepciones Win64, catch, finally, throw, using y cleanup
```

Aquí la documentación debe seguir features completas a través de todas las capas. Describir la
sintaxis de una lambda sin mostrar environment, delegate cell, HIR, MIR, lowering y destrucción no
sería una reconstrucción.

## 8. Era F — productos, ABI y ownership, Stages 46–50

| Stage | Resultado |
|---:|---|
| 46 | static/shared libraries, `.lib`, DLL e import library |
| 47 | Stable C ABI y hosts C |
| 48.1 | C header generator |
| 48.2 | `.nsmeta` writer/reader/verifier |
| 48.3–48.6 | consumidores C, C++, C#/PInvoke y C++/CLI |
| 49 | RAII y move semantics |
| 50.1–50.10 | `owned`, `borrowed`, flow, escape, HIR/MIR/LLVM, metadata y cierre |

Stage 50 necesita varios capítulos. No debe comprimirse a “se añadieron qualifiers”: cambió el
modelo de tipos, firmas, transferencias, préstamo, control de flujo, cleanup y compatibilidad de
metadata.

## 9. Era G — biblioteca y maduración, Stages 51–57

### Stage 51

`String` completa, integración de ownership y correcciones encontradas al ejercer enums y runtime.

### Stage 52

Colecciones genéricas, `default(T)`, `var`, `foreach`, lowering por reescritura, ownership de
temporales y correcciones de double free. Las subdivisiones 52.x y 52.x2 deberán narrarse como una
revisión técnica, no como apéndices invisibles.

### Stages 53–55

Tiempo, entorno, archivos, paths, argumentos, enumeración, threads, atomics, mutex, monitor,
semaphore/event, proceso, hardware y diagnostics.

### Stage 56

Interop completion y sistema de tipos: function pointers, arrays embebidos, unions, packing,
aliases FFI, `ref/out/in`, miembros de structs, definite assignment y raíz `Object` sin boxing.

### Stage 57 / CC1 / CC2

Correctness del lenguaje existente: ternario, `do...while`, `switch`, asignaciones compuestas,
shifts, `stackalloc`, parsing y formatting numérico, vistas, I/O binaria, buffers, process spawn,
arenas, enums y consolidación semántica/backend.

## 10. Era H — versiones 0.7.x

El relato de versiones no sustituye los stages; agrega disciplina de producto.

```text
0.7.4  primer build instalado; G00 descubre que el orden de medición era inválido
0.7.5  G00 corregido; baseline certificado; cierre de constraints cross-project
0.7.6  gran entrega de enums, name lookup, ownership y constantes
```

El capítulo de 0.7.5 debe explicar por qué capturar stderr de LLVM exigió un supervisor padre/hijo
y por qué “último pass observado” no significa “pass culpable”.

## 11. Era I — 0.8.0-alpha

0.8 cerró workarounds que impedían programas reales:

- lifetime de temporales string;
- arrays nativos poseídos y length inferida;
- especialización genérica por miembro alcanzado;
- parámetros predeterminados;
- observabilidad y gates del toolchain;
- conformidad ABI, PDB/CodeView, COM, SIMD y otras áreas según el gate realmente aterrizado.

La investigación deberá separar el plan completo de `123_plan_implementacion_0_8_alpha.md` de lo
que `126_native_sharp_0_8_0_alpha_closure.md` certifica como cerrado.

## 12. Era J — 0.9.0

0.9 cambió la unidad de distribución. Ya no basta compilar fuentes juntas:

```text
proyecto productor
  → .lib / DLL
  → header / import library / NSMETA
  → proyecto consumidor independiente
  → semántica reconstruida
  → link físico
```

NSMETA evolucionó hasta 1.5 en el cierre 0.9 observado. Se añadieron clases opacas, ownership y
callbacks precompilados. El producto siguió distinguiendo contrato semántico de símbolos físicos.

## 13. Era K — 1.0 E1–E4

Las primeras megaentregas hacia 1.0 ya están documentadas y certificadas:

| Entrega | Contrato |
|---|---|
| E1 | objetos públicos precompilados completos, constructors/destructors y descriptores |
| E2 | templates públicos, constraints y genéricos cross-library |
| E3 | lifetime de delegates/closures y excepciones tipadas entre módulos |
| E4 | Native ABI 1.0, Runtime ABI 1.0, NSMETA 1.8 y golden ABI |

El baseline G00 observado registró 11/11 etapas, 324 casos, 0 fallos, 1013 archivos bajo huella,
`S0 == S1` y binario construido idéntico al instalado.

## 14. Era L — estabilización y producto 1.0.0

Las fuentes actuales describen este tramo como trabajo siguiente, no como cierre consumado:

```text
0.9.0 + E1–E4 certificado
  ↓
ampliar corpus consumidor de .lib y DLL
  ↓
diagnósticos precisos para incompatibilidades ABI
  ↓
reproducción desde instalación limpia
  ↓
1.0.0-rc.1
  ↓
periodo de estabilización y correcciones sin romper ABI
  ↓
gate final de publicación
  ↓
nsc 1.0.0
```

La obra marcará esos capítulos como `PLANEADO` hasta que existan artefactos. Si 1.0 se implementa
mientras se redacta el libro, los capítulos se actualizarán con la evidencia real y nunca con el
plan antiguo como sustituto.

## 15. Línea de tiempo compacta

```text
idea y promesa
  → contratos 00–05
  → frontend 06–10
  → IR y backend 11–20
  → proyectos y OOP 21–30
  → herencia/RTTI 31–39
  → valores/callables/EH 40–45
  → libraries/ABI/ownership 46–50
  → Core/madurez/correctness 51–57
  → 0.7.x producto medible
  → 0.8 toolchain utilizable
  → 0.9 bibliotecas precompiladas
  → E1–E4 ABI congelado
  → RC y 1.0.0 público
```

## 16. Cronología material incorporada en Rev2

El archivo raíz concentra material fechado principalmente entre el 16 y el 31 de agosto de 2026,
pero esa ventana describe la colección observada, no necesariamente todos los días efectivos de
desarrollo. Muchos árboles fueron copiados o extraídos y conservan timestamps con semánticas
distintas. La cronología de la obra se apoyará primero en contenido, manifests, worklogs y
handovers; el timestamp servirá como una evidencia adicional.

| Ola material | Anclas observadas | Lectura histórica correcta |
|---|---|---|
| Diseño 00–03 | `NativeSharp_Parte1_snapshot_00_03` | Último paquete temprano observado sin árbol del compilador. |
| Arranque 00–06 | `NativeSharp_snapshot_00_06` | Primer árbol C++ conservado; ya contiene más que un `main` vacío. |
| Frontend 07–10 | `NativeSharp_delivery_07_10` | Una sola entrega física que Rev2 divide didácticamente. |
| IR/backend 11–19 | `NativeSharp_delivery_11` … `19` | Secuencia acumulativa con manifests y resultados históricos. |
| Capstone 20 | `NativeSharp_Parte1_Proyecto_Completo` | Stage 20 consolidado, sin carpeta `delivery_20` observada. |
| OOP 21–30 | `NativeSharp_Entrega21_*` … `Entrega30_*` | Crecimiento acumulativo; 21/22 carecen de CTest integral registrado. |
| Parte 3 no lineal | MacroA, Stage37, Recoveries35/36, Stage38 | La genealogía física no sigue el orden numérico. |
| Lenguaje avanzado 39–45 | Stage39 … subetapas 45.12 | Hay descripciones obsoletas y no aparece un paquete 45.8 nominal. |
| Producto/ownership 46–50 | FULL, SOURCE_ONLY, CANDIDATE, handovers | El tipo de empaquetado forma parte de la evidencia. |
| Core/correctness 51–56 | Stage51, Stage52/x, Stage53–56 | Aparecen revisión crítica, ejecución Wine y entregas doble ZIP. |
| Ecosistema | NSASM, NSLink, nsbind, bindings y ejemplos | Consumidores y autoridades externas; no hijos automáticos de `nsc`. |

## 17. Correcciones a una lectura puramente lineal

### 17.1 Inicio

La obra empezará desde un directorio vacío porque debe ser reconstruible. Sin embargo, el corte
vacío y los primeros microincrementos serán estados pedagógicos marcados `RECONSTRUIDO`; la primera
convergencia contra un artefacto conservado ocurrirá en Stage 06.

### 17.2 Fronteras de entrega

Los números 07–10 y 20 son fronteras lógicas útiles, pero no corresponden a cuatro paquetes
separados más un `delivery_20`. Rev2 mostrará la unidad física original y el método usado para
descomponerla.

### 17.3 Recuperaciones

Stages 35 y 36 se narrarán donde corresponden funcionalmente, pero su diagrama físico mostrará que
son recoveries sobre Stage37. Stage38 es el checkpoint consolidado que sigue a esa operación.

### 17.4 Versiones y etiquetas

La presencia de `0.1.0` en Stage50 después de estados `0.2.0`, o de descripciones de Stage41 en
Stages42/43, demuestra que versión declarada, stage nominal y contenido deben catalogarse por
separado. Ningún reloj corrige automáticamente a otro.

## 18. Enlaces de control Rev2

- La genealogía detallada vive en
  [`09_GENEALOGIA_FISICA_DE_SNAPSHOTS_REV2.md`](09_GENEALOGIA_FISICA_DE_SNAPSHOTS_REV2.md).
- Las ampliaciones que se insertarán en los capítulos viven en
  [`10_AMPLIACIONES_DEL_INDICE_MAESTRO_REV2.md`](10_AMPLIACIONES_DEL_INDICE_MAESTRO_REV2.md).
- La correspondencia capítulo–fuente vive en
  [`11_MATRIZ_FUENTES_HISTORICAS_A_CAPITULOS.md`](11_MATRIZ_FUENTES_HISTORICAS_A_CAPITULOS.md).
