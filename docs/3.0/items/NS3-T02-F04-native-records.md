# NS3-T02-F04 — Native records

## Estado

TERMINADA.

## Qué se implementó y endureció

- `record struct` mantiene generación puramente estática y derives independientes para
  `Equality`, `Hash`, `Print` y `Clone`; no incorpora identidad runtime.
- Igualdad/hash validan recursivamente sus campos: un record anidado debe derivar el mismo
  contrato y una unión anidada se rechaza con la ruta exacta del campo.
- `Clone` rechaza almacenamiento `owned` directo o anidado antes de HIR, evitando copias que
  produzcan doble propietario.
- Records vacíos bajan correctamente: igualdad constante verdadera y hash FNV-1a determinista.
- NSMETA sube a 1.26 y publica cinco flags de tipo: identidad record y los cuatro derives. El
  materializador de `LibraryReference` reconstruye `record struct ... derive(...)`.
- El validador rechaza derives sin marca record, record sobre tipo no-struct/union y flags record
  falsamente etiquetados como NSMETA anterior a 1.26.
- La proyección ABI continúa excluyendo clausura binaria privada 1.25, pero conserva los flags
  públicos de record. Se archivó el golden legible `historical-1.25.nsmeta`.

## Archivos nuevos

- `tests/unit/NativeSharp3T02NativeRecordTests.cpp`: doce checks dirigidos de ejecución O0,
  negativas semánticas, metadata malformada/downgrade y consumidor precompilado.
- `tests/fixtures/nsmeta/historical-1.25.nsmeta`: último formato anterior, ahora histórico.
- `docs/3.0/language/native-records.md`: contrato bilingüe de sintaxis, semántica y costos.
- Este documento: evidencia del ítem y de su validación.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: elegibilidad recursiva de derives.
- `src/Frontend/ProjectOopHirModel.cpp` y `src/LLVM/ProjectOopLlvmLowerer.cpp`: records vacíos.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp` y
  `src/Driver/Driver.cpp`: NSMETA 1.26, validación y rehidratación precompilada.
- Contratos/expectativas de versión en `src/Driver` y sus unitarias dirigidas.
- `CMakeLists.txt`: target `nsc_ns3_t02_native_record_tests`.
- Resúmenes históricos T02: fronteras de F04 y NSMETA actualizadas.

## Mejora obtenida

Los records dejan de depender de que productor y consumidor compartan el AST fuente. Su política
de derives ya es superficie pública versionada, los casos vacíos tienen identidad matemática
estable y ninguna expansión puede atravesar silenciosamente un record no comparable o duplicar
ownership. Todo permanece en el ABI de un struct Win64 ordinario y sin helpers runtime.

## Validación dirigida

- Build local de `nsc` y del target exclusivo F04: correcto.
- `nsc_ns3_t02_native_record_tests`: PASS 12/12, productos NativeSharp O0 y NSLink local.
- `nsc_ns3_t02_aggregate_tests`: PASS 13/13.
- `c58_ns3_records`: compilación O0 con el `nsc.exe` local, enlace NSLink y exit 42.
- Contratos unitarios afectados: migración 15/15, matriz 17/17, breaking ledger 15/15,
  RC freeze 10/10 y compatibilidad final 16/16.
- Golden NSMETA 1.25 generado con el compilador local y verificado por el lector 1.26.
- Sin baseline, CTest agregado, instalación, deploy ni validación O1/O2/O3.
