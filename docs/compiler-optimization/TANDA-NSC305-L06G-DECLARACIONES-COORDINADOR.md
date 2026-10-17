# Tanda NSC 3.0.5 L06G - declaraciones del coordinador

## SCOPE LOCK

- Objetivo exacto: hacer que el coordinador LLVM respete su lista calculada de declaraciones.
- Revisión de partida: `81f722a85`.
- Evidencia: L06F redujo la lista del coordinador de `8.602` a `12`, pero la instrumentación por
  fase atribuyó `860.010 us` de `~933.000 us` a `declare-functions`.
- Causa: `declareFunctions` aplicaba `functionDeclarations_` solamente cuando la partición no era
  dueña de globales. El coordinador recibía la lista correcta pero la condición
  `!ownsModuleGlobals_` la anulaba y materializaba el catálogo completo.
- Corrección autorizada: aplicar el filtro a toda partición que tenga una lista explícita; la
  clausura L06F conserva funciones locales y callees de inicializadores estáticos.
- Protegido: sintaxis, ABI, NSMETA, nombres, wrappers, vtables, cctors, imports usados, COFF y
  caché.
- Pruebas: cross-library con inicializador estático importado, lowering estructural, verifier,
  enlace/ejecución, cinco cold sin hits y una warm completa.

## Resultado

La condición se corrigió para que cualquier partición con lista explícita, incluido el
coordinador, aplique `functionDeclarations_`. El primer rojo posterior fue útil y específico: el
coordinador ya no encontraba `Object.ToString` para la vtable local. La clausura se completó con
los callees de slots virtuales, slots de interface y cadenas de destrucción de tablas locales.

Se conservó instrumentación temporal marcada `NSC305-L06G` en
`A:\temp\nsc-llvm-lowering-phases.log`. El archivo se trunca una vez por proceso y atribuye cada
fase por módulo sin saturar la consola de la GUI.

## Validación focal

- `nsc_cross_library_inlining_tests`: `PASS 16/16`; incluye un inicializador estático local que
  llama una función precompilada, enlaza y ejecuta con exit 0.
- `nsc_project_oop_lowering_tests`: `PASS 108/108`.
- `nsc.exe` reconstruido y contrato de host runtime estático aprobado.
- Binarios cold y warm del programa mínimo: exit 0.
- Warm: `9 hits / 0 misses`, LLVM `123.620 us`.

## Matriz cold sin hits

| Corrida | LLVM total (us) | Total (ms) |
|---:|---:|---:|
| 1 | 205.893 | 4.327 |
| 2 | 212.291 | 4.356 |
| 3 | 154.227 | 4.279 |
| 4 | 208.067 | 4.338 |
| 5 | 149.999 | 4.299 |

Medianas:

- frente a L06F: LLVM `977.961 -> 205.893 us` (`-78,9 %`);
- frente al inicio del catálogo amplio: LLVM `3.843.405 -> 205.893 us` (`-94,6 %`);
- total focal `~8.102 -> 4.327 ms` (`-46,6 %`).

Después de la corrección, `declare-functions` del coordinador bajó de `860.010 us` a `630 us` en
la prueba de atribución. LLVM dejó de ser el cuello dominante del programa mínimo amplio. Ahora
dominan `generics.visibility`, `names` y el enlace; el proyecto real debe confirmar la misma caída
antes de cerrar L06 general.

Evidencia:

- atribución roja: `A:\temp\nsc305-l06-largecatalog\l06g-phase-attribution2`;
- rojo de vtable: `A:\temp\nsc305-l06-largecatalog\l06g-filter-one`;
- verde con clausura completa: `A:\temp\nsc305-l06-largecatalog\l06g-filter-two`;
- cinco cold y warm: `A:\temp\nsc305-l06-largecatalog\l06g-five-cold`;
- pruebas: `A:\temp\nsc305-l06g-tests2`.
