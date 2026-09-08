# 09 — Genealogía física de snapshots — Revisión 2

## 1. Convención

Esta genealogía distingue:

- **orden lógico:** número pedagógico del stage;
- **orden material:** artefacto del que realmente parte otro;
- **estado observado:** archivo/directorio presente;
- **certificación histórica:** lo que el worklog afirma haber ejecutado;
- **recertificación Rev 2:** todavía pendiente salvo indicación expresa.

## 2. Frontera de diseño, 00–03

| Artefacto | Contenido | Código | Clasificación Rev 2 |
|---|---|---:|---|
| `NativeSharp_Parte1_snapshot_00_03` | docs 00–03, curso, README, worklog, manifest | no | `OBSERVADO_EN_ARCHIVO` |
| `md/00..03` | copias sueltas de los contratos | no | pendiente comparar hashes |

El worklog declara que no existen SourceManager, lexer, parser, AST, Sema ni backend. Éste es el
último checkpoint original anterior al código.

## 3. Primer código, Stage 06

| Artefacto | Archivos esenciales | Evidencia histórica |
|---|---|---|
| `NativeSharp_snapshot_00_06` | CMake, main, Driver, TargetInfo, TargetDataLayout, test | GCC 4/4; Clang 4/4; Werror |

No existe un snapshot original con sólo `main` vacío. La obra creará una cadena pedagógica
`C000..C006` extrayendo piezas de Stage 06, siempre marcada `RECONSTRUIDA`.

## 4. Frontend conjunto, 07–10

| Artefacto | Archivos | Docs | Manifest | Evidencia conservada |
|---|---:|---:|---:|---|
| `NativeSharp_delivery_07_10` | 116 | 8 | 1 | GCC, Clang, sanitizer, CLI, unit counts |

No se inventarán ZIP 07, 08, 09 y 10. Los checkpoints didácticos serán deltas derivados y
reproducidos desde la entrega conjunta.

## 5. Parte 1 incremental, 11–20

| Stage | Artefacto físico | Archivos | CTest histórico localizado |
|---:|---|---:|---|
| 11 | `NativeSharp_delivery_11` | 123 | 29/29 GCC, Clang, ASan+UBSan |
| 12 | `NativeSharp_delivery_12` | 158 | 36/36 baseline que Stage 13 revalida |
| 13 | `NativeSharp_delivery_13` | 183 | 43/43 final |
| 14 | `NativeSharp_delivery_14` | 204 | 48/48 GCC, Clang, sanitizers |
| 15 | `NativeSharp_delivery_15` | 238 | 53/53 GCC, Clang, sanitizers |
| 16 | `NativeSharp_delivery_16` | 288 | 61/61 final; una corrida previa 56/57 no se ocultará |
| 17 | `NativeSharp_delivery_17` | 290 | 68/68 |
| 18 | `NativeSharp_delivery_18` | 326 | 70/70; 11/11 COFF y reassembly |
| 19 | `NativeSharp_delivery_19` | 379 | 76/76; 10/10 corpus PE estructural |
| 20 | `NativeSharp_Parte1_Proyecto_Completo` | por medir sin anidados | 76/76 + matrices O0–O3 documentadas |

Stage 19 no ejecutó los PE por falta de Windows/Wine. Eso se conserva como
`PASS_STRUCTURAL_ONLY`, no se reescribe como ejecución.

## 6. Parte 2, 21–30

| Stage | Archivos observados | Evidencia histórica destacada |
|---:|---:|---|
| 21 | 461 | full CMake/CTest no ejecutado; dependencies unavailable |
| 22 | 489 | full CMake/CTest no ejecutado; dependencies unavailable |
| 23 | 540 | 87/87 GCC, Clang y exact-package tree |
| 24 | 557 | 89/89; manifest 556/556 antes/después de extraer |
| 25 | 591 | 92/92 GCC, Clang, ASan+UBSan |
| 26 | 610 | 95/95; focused y driver |
| 27 | 635 | 98/98; clean source copy; manifest |
| 28 | 659 | 101/101; package extraction |
| 29 | 681 | 103/103; worklog imprime accidentalmente `stage 28` en una línea |
| 30 | 715 | 106/106; OOP lowering 55/55; driver 166/166 |

La línea errónea de identidad en el worklog de Stage 29 se conservará como error documental y se
comparará con `Version.h` y `nsc --version` del snapshot.

## 7. Parte 3, orden lógico y orden material

### Orden lógico

```text
31 → 32 → 33 → 34 → 35 → 36 → 37 → 38
```

### Orden material observado

```text
MacroA 31–34
  → Stage37 Static Initialization
  → Stage35 Recovery on Stage37
  → Stage36 Recovery on Stage37+35
  → Stage38
```

### Evidencia

| Artefacto | Archivos | Resultado histórico |
|---|---:|---|
| Macro A 31–34 | 802 | 116/116; focused 63/63 |
| Stage 37 | 825 | 121/121; focused 56/56 |
| Stage 35 Recovery | 856 | 126/126; focused 80/80 |
| Stage 36 Recovery | 891 | focused 95/95; manifest 855/855 previo |
| Stage 38 | 923 | 138/138 GCC/Clang/ASan/UBSan; focused 94/94 |

## 8. Stages 39–44

| Stage | Archivos | Identidad documentada | Evidencia histórica |
|---:|---:|---|---|
| 39 | 961 | `0.2.0-dev39` | focused 93/93; regresiones 38/36/35 |
| 40 | 1,000 | `0.2.0-dev40` | focused 61/61; manifest 960/960 previo |
| 41 | 1,061 | `0.2.0-dev41` | focused 72/72; baseline 151/151 |
| 42 | 1,085 | `0.2.0-dev42` | cinco gates; CMake description obsoleta |
| 43 | 1,123 | `0.2.0-dev43` | 164/164 GCC/Clang/ASan/UBSan |
| 44 | 1,166 | `0.2.0-dev44` | focused 73/73; 170/170 completo |

## 9. Stage 45 granular

| Substage | Árbol observado | Archivos | Rol |
|---:|---|---:|---|
| 45.1 | sí | 1,169 | syntax/AST |
| 45.2 | sí | 1,175 | exception Sema |
| 45.3 | sí | 1,183 | cleanup scopes |
| 45.4 | sí | 1,193 | finally flow |
| 45.5 | sí | 1,201 | using/IDisposable |
| 45.6 | sí | 1,213 | HIR exceptional CFG |
| 45.7 | sí | 1,226 | MIR exceptional CFG |
| 45.8 | no con nombre propio | — | native exception runtime, por localizar en delta |
| 45.9 | sí | 1,267 | LLVM Win64 EH básico |
| 45.10 | sí | 1,294 | cleanup funclets |
| 45.11 | sí | 1,312 | typed catch/RTTI |
| 45.12 | sí | 1,339 | linker/PE/EH |

El salto 1,226→1,267 es la primera zona donde buscar el cambio absorbido de 45.8.

El chat histórico añade un cierre lógico 45.19: 45.13–45.19 se agruparon como validación,
hardening, empaquetado y handover, no como entregas fuente separadas. El nodo físico
`NativeSharp_Parte3_Stage45_Final_Certified.zip` quedó vinculado al chat por coincidencia SHA-256
exacta: `a106c777c76652969ceaaf236f6335e9b5db7f71d9388e9a585ddcbcbf09bef5`.

## 10. Productos y ownership, 46–50

| Tramo | Forma material observada | Precaución |
|---|---|---|
| 46 | FULL ZIP + changed-files ZIP | no hay extracción raíz homónima observada |
| 47 | FULL/SOURCE_ONLY, con copias `(1)` | comprobar hash, no tamaño solamente |
| 48.1 | CANDIDATE + FULL + SOURCE_ONLY | candidate no es final |
| 48.2 | FULL | localizar incremental dentro de handover si existe |
| 48.3–48.6 | FULL/SOURCE_ONLY | varios hosts y dependencias de Windows |
| 49 | FULL/SOURCE_ONLY y handover | descripción CMake heredada incorrecta |
| 50.1–50.11 | FULL/SOURCE_ONLY por checkpoint | cambio aparente de versioning y anomalías de package |
| 50 final | FULL/SOURCE_ONLY | baseline para Stage 51 |

## 11. Biblioteca y correctness, 51–56

La genealogía deja de ser “un ZIP por stage” y se vuelve una red:

```text
Stage51_SOURCE_ONLY / FULL / Package / FINAL_Package
  → Stage52_Package / Cierre_Completo / foreach / revisión / cierre formal
  → Stage53.x por operación
  → Compilador_Completo sucesivo
  → Stage54.x y Stage55
  → Stage56 C1..C8
  → auditorías y bloques CC1/CC2
```

La obra debe enseñar por qué este cambio de forma ocurrió, no esconderlo bajo una numeración lineal.

## 12. Tabla de autoridad

Para cada transición se buscarán, en este orden:

1. ZIP de salida;
2. manifest del ZIP;
3. extracción que coincida con el ZIP;
4. worklog específico;
5. matriz de pruebas específica;
6. diff/changed-files frente a la base;
7. logs con identidad del binario;
8. conversación que explica decisiones;
9. snapshot posterior como corroboración acumulativa.

## 13. Vacíos reales ya identificados

- checkpoint original anterior a Stage 06 con `main` vacío;
- ZIP separado de Stage 20;
- CTest completo contemporáneo para Stages 21/22;
- snapshots físicos originales 35/36 antes del recovery — por definición no persistieron;
- paquete con nombre 45.8;
- algunas extracciones raíz de 46–48;
- una identidad de versión coherente durante Stage 50;
- separación limpia de builds en FULL 50.5;
- contenido incremental de SOURCE_ONLY 50.11.

Estos vacíos serán capítulos o notas forenses. No se rellenarán silenciosamente.
