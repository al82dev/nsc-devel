# NSC 3.0.5 — D04: publicación de artefactos

## Objetivo

Extraer del `Driver` la coordinación transaccional de los productos objeto, ejecutable,
biblioteca estática y biblioteca dinámica sin cambiar generación, enlace, diagnósticos,
códigos de salida, nombres de archivos, ABI ni formato NSMETA.

## Hipótesis causal

Las cuatro rutas de producto repetían directamente la preparación, el registro de companions y
el commit de `basic::ArtifactTransaction`. Esa duplicación ya había producido cuatro variantes
manuales para PDB, estado incremental, encabezado, metadata, import library y runtimes. Una
estrategia pequeña por producto hace explícitas esas diferencias y conserva un único dueño de la
transacción completa.

## Frontera aplicada

`src/Driver/ArtifactPublisher.cpp` contiene estrategias no recursivas para:

- objeto: destino primario y commit;
- ejecutable: destino primario, seed incremental de PE/estado/PDB, companions opcionales y
  runtimes;
- biblioteca estática: `.lib`, `.h` y `.nsmeta`;
- biblioteca dinámica: DLL, import library, `.h`, `.nsmeta`, PDB opcional y runtimes.

Permanecen fuera del publicador:

- lowering LLVM y generación de objetos;
- construcción PE/COFF y verificación de productos;
- cálculo de exports;
- generación del encabezado C;
- construcción y verificación de metadata;
- cálculo del cierre de runtimes;
- rendering de reportes del driver.

La emisión independiente de encabezados y metadata ya delegaba su escritura transaccional a
`writeTextTransactionally` y `writeFileTransactionally`; D04 no duplica esa responsabilidad.

## Contratos preservados

- El stage primario conserva el basename final requerido por PE, CodeView e import libraries.
- `.pdb`, `.nslinc`, `.h`, `.nsmeta` y el fallback `.import.lib` mantienen su derivación previa.
- Un estado incremental ausente representa la ruta cold y no se registra como artefacto.
- Los runtimes se publican en el directorio del producto con su basename original.
- El orden de registro y commit de cada conjunto permanece igual.
- El `Driver` conserva todos los prefijos de error, nombres de fase, stdout y códigos de salida.
- La destrucción del publicador conserva la limpieza y rollback de `ArtifactTransaction`.
- D04 no interpreta ni modifica NSMETA.

## Prueba focal

`tests/unit/ArtifactPublisherTests.cpp` prueba directamente, sin duplicar el compilador:

1. stage y commit del objeto;
2. seed warm y publicación conjunta de ejecutable, PDB, `.nslinc` y runtime;
3. rechazo atómico de una biblioteca estática con un companion faltante;
4. nombres y publicación conjunta de DLL, import library, header, metadata y PDB;
5. fallback de import library cuando el destino primario ya termina en `.lib`.

La infraestructura registra:

- target `nsc_artifact_publisher_tests`;
- CTest `nsc.artifact-publisher`;
- `ArtifactPublisher.cpp` exactamente una vez en `nsc_driver`.

## Gates de cierre

- build de `nsc` y `nsc_artifact_publisher_tests`;
- CTest focal `nsc.artifact-publisher` 1/1;
- regresiones focales existentes de long path, enlace incremental y productos DLL/NSMETA;
- cero autorrecursiones y helpers de D04 de hasta 60 líneas;
- source-registration único;
- `git diff --check` limpio;
- commit local aislado de D04.

No se reclama baseline ni suite completa en esta tanda.

## Evidencia de cierre y atribución causal

Validación focal ejecutada con `TEMP` y `TMP` en `A:\temp` y con
`NSC_WINDOWS_RUNTIME_LIBRARY` apuntando al runtime recién generado dentro del build de A:

- build del target oficial `nsc`: PASS;
- build de `nsc_artifact_publisher_tests`: PASS;
- CTest `nsc.artifact-publisher`: PASS 1/1, 0.07 s;
- source de `ArtifactPublisher.cpp` registrado una sola vez en `nsc_driver`;
- helpers de D04 de hasta 60 líneas y cero autorrecursiones;
- `git diff --check`: sin errores.

NSLink quedó resolviendo por nombre desde el `PATH` nativo de Windows. La matriz focal terminó:

- `nsc.ns3.t34.long-path-publication`: PASS;
- `nsc.item392-incremental-link`: PASS;
- `nsc.stage48_1.c-header`: PASS;
- `nsc.stage48_2.library-metadata`: 393/398 checks PASS;
- `nsc.stage46.native-libraries`: 441/451 checks PASS.

Los dos targets globales que no terminaron verdes fueron auditados antes del cierre:

1. Stage 48.2 calcula el fingerprint fallido directamente con `LibraryMetadataCodec`; no entra a
   `Driver` ni a `ArtifactPublisher`. El mismo target intenta abrir los fixtures
   `historical-1.38.nsmeta` y `historical-1.39.nsmeta`, ausentes del árbol y sin historial Git.
   Estas causalidades quedaron registradas como GATE-F01 y GATE-F02.
2. Stage 46 llega a NSLink, pero diez checks fallan por símbolos estáticos/constantes importados
   no resueltos. El fallo ocurre dentro de `buildDynamicObjects` o `buildObjects`, antes de generar
   companions o llamar a `ArtifactPublisher::commit`. D04 conserva el mismo
   `ArtifactTransaction::add`, el mismo orden de destinos y los mismos basenames de stage que el
   código previo. Esta causalidad quedó registrada como GATE-F03.

Por tanto esos defectos no se atribuyen ni se ocultan dentro de D04. La hoja cierra con sus gates
de publicación verdes y deja cada defecto descubierto como hoja absoluta independiente.
