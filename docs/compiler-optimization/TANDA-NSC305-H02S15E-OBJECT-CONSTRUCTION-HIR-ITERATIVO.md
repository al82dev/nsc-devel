# NSC 3.0.5 — H02S15E: construcción ordinaria HIR iterativa

## SCOPE LOCK

- Objetivo: eliminar la reentrada de pila nativa de construcción ordinaria por storage,
  argumentos e initializers de fields implícitos.
- Incluido: `ObjectConstruction`, `ValueConstruction` y raw `PlacementConstruction`, conversiones,
  packs, defaults, fresh-owner borrow, cleanup y fields base-a-derivado.
- Conservado: selección Stage 28, diagnósticos, orden físico/evaluación, types, identities,
  ownership, `argumentCount`, provenance y `lastErrorRange`.
- Protegido: allocator H02S15D, aggregate/tuple/with H02S15F, statements H02S16, parser, Sema,
  MIR, LLVM, ABI, API pública y NSMETA.
- Target único: `nsc_project_oop_lowering_tests`.
- Selector único: `hir-object-construction-depth`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalación y tracking global.

## Causa y diseño

La rama ordinaria histórica bajaba recursivamente el storage raw-placement, cada argumento y cada
initializer de field. `OrdinaryConstructionFrameState` conserva un único nodo parcial y avanza en
la worklist común por `Storage`, `Arguments`, `FieldInitializers` y `Ready`. Cada reanudación
consume exactamente un child en orden fuente. Defaults y pack vacío se materializan de forma
síncrona después de los explícitos; `argumentCount` se congela antes de fields. El finalizador
únicamente aplica el wrapper `FullExpressionCleanup` ya decidido y no llama a ningún lowerer.

El clasificador usa el complemento exacto de allocator completo y absorbe toda construcción
ordinaria válida y resuelta, incluso `new()` sin argumentos ni fields y las colas síncronas de sólo
defaults o pack vacío. Si el frame queda `Ready` sin child, se finaliza y entra directamente al
unwind común. La rama histórica es únicamente un guard de escape y rechaza cualquier construcción
ordinaria resuelta que no haya atravesado el frame. El acceso adicional de prueba a
`ProjectInheritanceModel` es un `friend` privado y no amplía API.

## Contratos ejercitados por el selector

- cadenas reales de 32,768 aristas construction→argument y placement→storage;
- class `Owned/Owned`, value struct con ownership por defecto y raw placement `Owned/Borrowed`;
- storage antes de argumentos, address context ordinario, orden, conversiones y named ordinals;
- diagnóstico de ordinal ausente después de bajar/convertir el segundo argumento y antes del
  tercero, demostrado mediante ordinales de borrow/cleanup;
- pack expandido único y sus tres errores; defaults posteriores y sus tres errores; pack vacío
  posterior a defaults y sus tres errores;
- borrows anidados con reserva post-child y un cleanup por construcción;
- target-typed `new()` e identidades de constructor;
- hojas class/struct, defaults-only y empty-pack-only atravesando el frame sin child productivo;
- implicit default de clase con fields base-a-derivado, orden de miembros, `Assignment(Equal)`,
  `ThisValue`, volatile, conversión y construcción anidada en la misma worklist;
- error exacto de conversión perdida;
- exclusión de allocator completo y binding inválido;
- fallo interior tras 4,096 frames, rango más interno y ausencia de nodo parcial.

## Evidencia focal

- Build: `A:\NSC-devel-paralelo\logs\agente-1\h02s15e-build-final.log`.
- Selector: `A:\NSC-devel-paralelo\logs\agente-1\h02s15e-focus-final.log`.
- Resultado: `PASS: 21 checks` (exit code 0).
- No se ejecutará ninguna otra prueba.

La inspección estática conserva `wrapImplicitConversion` antes de validar el ordinal del parámetro.
El satélite runtime hace observable que el argumento problemático se bajó y que el siguiente no se
agendó mediante los ordinales de borrow/cleanup; no agrega instrumentación artificial al producto.

## Criterio de cierre

El target exacto compila, el selector único pasa, la auditoría estática encuentra cero reentradas
productivas en la ruta ordinary construction y el diferencial no invade S15F ni S16. El cierre de
S15E desbloquea S16A; no implementa ni valida ninguna hoja S16.
