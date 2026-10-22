# NSC — Matriz de cobertura de hallazgos de rendimiento

**Fecha:** 2026-10-21

**Rama y revisión auditadas:** `development` en `5b6582648661fdf78c32d267dca900edca1fffa5`

**Tracking canónico:** `NSC-PENDIENTES-Y-TERMINADOS-RENDIMIENTO.md`

**Denominador vigente:** `0 / 670`

## 1. Qué garantiza esta matriz

El número de filas del tracking no demuestra cobertura. La cobertura sólo se acepta cuando se cumplen a la
vez estos gates:

1. Todo hallazgo accionable del corpus fuente apunta a uno o más IDs `PERF-*`.
2. Todo ID `PERF-*` apunta a un hallazgo, una precondición, un gate o una validación del corpus.
3. Cada implementación identifica el símbolo actual en disco justo antes de editar.
4. Un hallazgo descartado conserva una razón verificable; no desaparece del inventario.
5. Una discrepancia entre documentos se registra y se resuelve; no se elige silenciosamente una versión.
6. Si la auditoría de una fila de inventario descubre trabajo nuevo, primero crece el denominador y después
   se implementa.

Ecuaciones de cierre documental:

```text
hallazgos_accionables_sin_ids == 0
ids_sin_procedencia == 0
hallazgos_descartados_sin_razon == 0
contradicciones_sin_registro == 0
filas_tracking == ids_tracking_unicos
```

El gate de rendimiento es distinto: sólo se cierra con medición controlada antes/después. Esta matriz no
afirma que exista una mejora medida ni que algún ítem esté implementado.

## 2. Corpus fuente congelado

- `NSC-ANALISIS-RENDIMIENTO-POR-FUNCION.md`: R-01..R-15, apéndice A y ranking del apéndice B.
- `Claude outputs/.../NSC-RENDIMIENTO-COMPILACION-Y-CACHES.md`: causas 1..6 y propuestas P1..P9.
- `Claude outputs/.../NSC-GUIA-DE-IMPLEMENTACION.md`: B0..B9 y H-1..H-9.
- Código actual bajo `src/`, usado para confirmar que los símbolos suplementarios siguen presentes.

Los anexos de refactor modular no forman parte del denominador de rendimiento salvo que un hallazgo de
rendimiento los cite expresamente. El IDE y NSC Builder no reciben implementación en esta campaña; las
necesidades de caché global se resuelven del lado del compilador.

## 3. Análisis por función — trazabilidad principal

| Hallazgo fuente | Qué cubre | IDs del tracking |
|---|---|---|
| R-01 | Identidades de tipo reconstruidas | `PERF-0001..PERF-0004-B` |
| R-02 | Consultas lineales por identidad de tipo y consumidores | `PERF-0017..PERF-0048-B` |
| R-03.1 | Worker HIR completo por miembro | `PERF-0065..PERF-0071` |
| R-03.2 | Barridos globales de locales y cleanups | `PERF-0072..PERF-0081-B` |
| R-03.3 | Lookup de closures lambda | `PERF-0082` |
| R-04.1 | Copia de layouts por función MIR | `PERF-0060-A..PERF-0062-B` |
| R-04.2 | Lookup de field layout por miembro | `PERF-0049-A..PERF-0052-B` |
| R-04.3 | Lookup de layouts por `typeKey` | `PERF-0053-A..PERF-0056-B` |
| R-04.4 | ABI y awaited frames recorriendo layouts | `PERF-0057..PERF-0059` |
| R-05.1 | Comprobación LLVM duplicada | `PERF-0011` |
| R-05.2 | Ausencia de memo de tipos LLVM | `PERF-0063..PERF-0064-B` |
| R-06.1 | Identidades declaradas lineales | `PERF-0087..PERF-0091-B` |
| R-06.2 | Asignación anticipada de `GenericLookupScope::Cow` | `PERF-0092..PERF-0093` |
| R-06.3 | Copias de `children()` | `PERF-0094..PERF-0099` |
| R-06.4 | `cloneAliasType` con sustitución vacía | `PERF-0364..PERF-0368` |
| R-06.5 | Ocho pasadas completas del AST | `PERF-0369..PERF-0376` |
| R-06.6 | `originNamespace()` por valor | `PERF-0100` |
| R-06.7 | Strings y resolución duplicada de templates | `PERF-0101-A..PERF-0102` |
| R-06.8 | Canonicalización y recursión de visibilidad | `PERF-0103`, `PERF-0413..PERF-0415` |
| R-07.1 | SHA-256 en memoria y archivo | `PERF-0005..PERF-0009-B` |
| R-07.2 | Procedencia calculada sin solicitarla | `PERF-0012` |
| R-07.3 | Restore, doble hash y lock reescrito | `PERF-0013..PERF-0014`, `PERF-0329..PERF-0338` |
| R-07.4 | Proyecto y grafo cargados repetidamente | `PERF-0083-A..PERF-0086-B`, `PERF-0396..PERF-0400` |
| R-07.5 | NSMETA leído y verificado repetidamente | `PERF-0015..PERF-0016-B`, `PERF-0339..PERF-0350`, `PERF-0444..PERF-0450` |
| R-07.6 | Copia profunda de dependencias binarias | `PERF-0466..PERF-0467` |
| R-08.1 | Runtime reconstruido en cada ejecutable | `PERF-0219..PERF-0233` |
| R-08.2 | NSLink por cada import library | `PERF-0234..PERF-0248`, `PERF-0459..PERF-0465` |
| R-08.3 | Lectura byte a byte de PE | `PERF-0010` |
| R-08.4 | Copia del texto IR para la llave runtime | `PERF-0468..PERF-0469` |
| R-08.5 | Destrucción completa al salir del CLI | `PERF-0401..PERF-0404` |
| R-09 | Trazas LLVM y runtime siempre activas | `PERF-0205..PERF-0218` |
| R-10.1 | Caché de superficie pública precompilada | `PERF-0249..PERF-0278` |
| R-10.2 | Publicación `O(M x T)` | `PERF-0110-A..PERF-0113-C` |
| R-10.3 | Ranking recalculado dentro del sort | `PERF-0114..PERF-0115-D` |
| R-10.4 | Materialización de contratos sin memo | `PERF-0116-A..PERF-0119-B` |
| R-10.5 | DFS de templates sin memo global | `PERF-0120-A..PERF-0121` |
| R-10.6 | Dispatch virtual lineal | `PERF-0122-A..PERF-0124-C` |
| R-10.7 | Diagnósticos construidos antes de error | `PERF-0125..PERF-0128` |
| R-11.1 | Tres hashes por keyword | `PERF-0104-A..PERF-0104-B` |
| R-11.2 | `lexAll` sin reserva | `PERF-0105` |
| R-11.3 | Copia y string por consulta de tipo del parser | `PERF-0106..PERF-0108`, `PERF-0416..PERF-0417` |
| R-11.4 | Lista de nombres conocidos reconstruida | `PERF-0109-A..PERF-0109-B` |
| R-11.5 | Unidad pública gigante limita paralelismo | `PERF-0377..PERF-0383` |
| R-12 | Tamaño y asignación individual de nodos AST | `PERF-0384..PERF-0395` |
| R-13 base | Índice autoritativo de miembros por tipo y consumidores | `PERF-0129-A..PERF-0138-B` |
| R-13.1 | Planes de interfaz por clase | `PERF-0139-A..PERF-0140` |
| R-13.2 | Copia del span exigida por la firma | Sin cambio: el análisis demuestra que la copia es el valor devuelto |
| R-13.3 | Lookup del accessor de evento | `PERF-0535..PERF-0537` |
| R-13.4/R-13.5 | Ordinales de Core repetidos | `PERF-0141-A..PERF-0148` |
| R-13.6 | Tipos por nombre simple | `PERF-0149-A..PERF-0151` |
| R-13.7 | Contenedores de shard creados por callable | `PERF-0405..PERF-0408` |
| R-13.8 | Extensiones por operador | `PERF-0152-A..PERF-0153` |
| R-13.9 | Carrier Optional y template por clave | `PERF-0154-A..PERF-0155` |
| R-14.1 | Identidades físicas precompiladas | `PERF-0156-A..PERF-0156-C` |
| R-14.2 | Escape summaries lineales | `PERF-0157-A..PERF-0159-B` |
| R-15.1 | Checksum de debug por unidad | `PERF-0160-A..PERF-0161-C` |
| R-15.2 | Bindings y slots de interfaz | `PERF-0162-A..PERF-0163-C` |
| R-15.3 | Poda recursiva en cada invocación | `PERF-0351..PERF-0363` |
| R-15.4 | Lowerer MIR con contenedores por función | `PERF-0409..PERF-0412` |
| R-15.5 | Plegado repetido de visibilidad | `PERF-0164-A..PERF-0165-D` |
| R-15.6 | `PartialTypeAggregator` | Resuelto causalmente por R-01; no genera una segunda implementación |
| R-15.7 | Especializaciones y template cerrado | `PERF-0166..PERF-0167` |
| R-15.8 | Inicializador estático lineal | `PERF-0168..PERF-0169` |
| R-15.9 | Imports externos y deduplicación de símbolos | `PERF-0170..PERF-0171-E` |
| R-15.10 | Funciones MIR por nombre | `PERF-0172..PERF-0174` |
| R-15.11 | Identidades y `find_if` de `ProjectGraph` | `PERF-0470..PERF-0471` |

## 4. Hallazgos suplementarios de los apéndices

La auditoría del 2026-10-21 encontró que estos P2 estaban escritos en el apéndice A pero no tenían fila
atómica propia. Ya forman parte del denominador; no se consideran cerrados.

| Símbolo actual confirmado | Problema | IDs |
|---|---|---|
| `resolveNameOfTemplateGenericMethod` | Recorre templates por `nameof` | `PERF-0498..PERF-0499` |
| `conflictingMutableReference` | Recorre todos los locales del shard | `PERF-0500..PERF-0502`, `PERF-0507` |
| `hasActiveBorrowOfOwnerLocal` | Recorre bindings, reasignaciones y vistas; crea `rootPlace` | `PERF-0503..PERF-0507` |
| `resolveCapabilityCalls` | Reescanea la expresión por descriptor capability | `PERF-0508..PERF-0511` |
| `findTraitImplementationOwner` | Recorre todos los templates | `PERF-0512..PERF-0515` |
| `inheritMoveNextDropFrames` | Bucle anidado por nombre MIR | `PERF-0516..PERF-0517`, `PERF-0519..PERF-0520` |
| `inheritConstructorFrames` | Bucle anidado por nombre MIR | `PERF-0516`, `PERF-0518..PERF-0520` |
| `findAttributeTarget` | Recorre tipos y miembros por atributo | `PERF-0521..PERF-0522`, `PERF-0525` |
| `ProjectMetaprogrammingModel::findType` | Lookup lineal por identidad | `PERF-0523`, `PERF-0525` |
| `ProjectMetaprogrammingModel::visibleMembers` | Reconstruye la vista pública | `PERF-0524..PERF-0525` |
| `finalizeExternalMetadataTypes` | Hipótesis de producto cartesiano todavía no demostrada | `PERF-0526..PERF-0527` |
| `resolveMemberPhysicalIdentity` | Fallback `O(M x F)` para constructors generator | `PERF-0528..PERF-0530` |
| `canonicalize` | Algunas claves asignan strings dentro del sort | `PERF-0531..PERF-0532`, `PERF-0534` |
| `canonicalizeNestedRecords` | Copias/ordenamientos P2 por medir | `PERF-0533..PERF-0534` |

Los renglones del apéndice A marcados como pasada única, colección local, falso positivo o backend legado no
se transforman automáticamente en cambios. Conservan esa clasificación fuente. El ranking de 150 funciones
del apéndice B es un inventario cuantitativo, no 150 diagnósticos; `PERF-0538` obliga a clasificar cada fila y
a aumentar el denominador antes de implementar si aparece otro hallazgo accionable.

## 5. Análisis de Claude — causas y propuestas

| Fuente | Cobertura en el tracking |
|---|---|
| Causa 1: host `/Od` | `PERF-0442..PERF-0443` |
| Causa 2: superficie de Core reprocesada | `PERF-0249..PERF-0278` |
| Causa 3: C01/C04 desconectadas e ineficaces | `PERF-0279..PERF-0328`, `PERF-0541` |
| Causa 4: runtime, import libs y enlace/PDB | `PERF-0219..PERF-0248`, `PERF-0459..PERF-0465`, `PERF-0480..PERF-0487` |
| Causa 5: debug full y doble SHA del restore | `PERF-0329..PERF-0338`, `PERF-0473..PERF-0479` |
| Causa 6: carpetas temporales vuelven fría la caché | `PERF-0451..PERF-0458`, `PERF-0472` |
| P1 host O2 | `PERF-0442..PERF-0443` |
| P2 `--time` y `--time-trace` | `PERF-0195..PERF-0204` |
| P3 caché por módulo precompilado | `PERF-0249..PERF-0278` |
| P4 rediseño C01/C04 | `PERF-0279..PERF-0328` |
| P5 runtime/import caches e import writer in-process | `PERF-0219..PERF-0248`, `PERF-0459..PERF-0465` |
| P6 restore por tamaño/mtime y lock estable | `PERF-0329..PERF-0338` |
| P7 prefix map y caché global | `PERF-0451..PERF-0458`, `PERF-0472..PERF-0479` |
| P8 instrumentación fija | `PERF-0205..PERF-0218` |
| P9 verificación NSMETA paralela | `PERF-0444..PERF-0450` |

## 6. Guía de Claude — B0..B9 y H-1..H-9

| Fuente | Resolución |
|---|---|
| B0 | Oráculo y medición basal: `PERF-0175..PERF-0194` |
| B1 | Host O2: `PERF-0442..PERF-0443`; lo ejecuta/valida Alberto |
| B2 | Medición CLI: `PERF-0195..PERF-0204` |
| B3/B4 | Instrumentación LLVM/runtime: `PERF-0205..PERF-0218`; conserva autorización contractual |
| B5 | Runtime e import libs: `PERF-0219..PERF-0248`, `PERF-0459..PERF-0465` |
| B6 | Superficie precompilada: `PERF-0249..PERF-0278` |
| B7 | C01/C04: `PERF-0279..PERF-0328`, `PERF-0541` |
| B8 | Restore: `PERF-0329..PERF-0338` |
| B9 | Debug paths/caché global: `PERF-0451..PERF-0458`, `PERF-0472..PERF-0479` |
| H-4/H-5/H-6 | Defectos concretos de C01/C04: `PERF-0279..PERF-0328`, `PERF-0541` |
| H-7 | Asimetría library/executable runtime cache: `PERF-0219..PERF-0233` |
| H-1/H-2/H-3/H-8/H-9 | Clasificación explícita en `PERF-0542`; no se mezclan automáticamente con rendimiento |

H-1 contiene 136 ciclos estáticos, no 136 pruebas de coste en ejecución. Sólo los ciclos identificados además
como ruta caliente tienen ítems de rendimiento (`PERF-0413..PERF-0420`). H-2 y H-3 son contratos/estilo; H-8
y H-9 pertenecen al refactor salvo que la medición demuestre una causa de rendimiento.

## 7. Contradicción histórica C01/C04

El handover 3.0.5 puede describir C01/C04 como completadas porque existen codecs, almacenamiento y restore.
El análisis actual demuestra otra cosa: la llave MIR se obtiene después de construir MIR, el resultado
restaurado se descarta y la caché de plantillas entra después del parseo. Por tanto:

- existencia de infraestructura: **HECHO histórico**;
- ahorro efectivo de frontend: **NO demostrado y actualmente ineficaz por diseño**;
- estado 3.0.6: `PERF-0279..PERF-0328` y `PERF-0541`, todos pendientes.

## 8. Estado de la garantía

- Filas del tracking: `670`.
- IDs únicos: `670`.
- Terminados: `0`.
- Hallazgos accionables explícitos sin ID: `0` después de añadir `PERF-0498..PERF-0542`.
- Ítems sin procedencia conceptual: `0` por inspección documental; el gate mecánico permanente es
  `PERF-0539`.
- Rendimiento certificado: **NO**. Falta R0, implementación y medición final controlada.

La garantía válida hoy es de **cobertura del plan**, no de velocidad obtenida. Si `PERF-0538` descubre un
nuevo hallazgo al clasificar el ranking bruto, el total `670` deja de ser válido y se incrementa antes de
continuar; no se permite cerrarlo como “incluido” dentro de un renglón genérico.
