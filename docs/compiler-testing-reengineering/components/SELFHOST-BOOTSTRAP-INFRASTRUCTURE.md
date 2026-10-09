# SelfHost.Bootstrap.Infrastructure — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `65d4dcf44143c7a945a786c8f1e6ba78d225ad02`.
- **Universo:** 74 archivos, 7556 líneas, 281 declaraciones/artefactos.
- **Disposición:** EXCLUIDA=1, INDIRECTA=73.
- **Diseño futuro:** 73 contratos y 438 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Conservar perfiles, locks, schemas y verificadores que demuestran las generaciones self-host sin aceptar fallbacks.

**Dependencias:** nsc0;nsc1;nsc2;NSLink;LLVM;Python;contratos self-host.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000470 | `selfhost/bootstrap/__init__.py` | `BOOTSTRAP_VERIFIER` | `EXCLUIDA` | `` | Verificador bootstrap init |
| PF-000471 | `selfhost/bootstrap/bootstrap-profile.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-001` | Dato normativo bootstrap bootstrap profile |
| PF-000472 | `selfhost/bootstrap/bootstrap.config.example.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-002` | Dato normativo bootstrap bootstrap.config.example |
| PF-000473 | `selfhost/bootstrap/bootstrap.lock` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-003` | Dato normativo bootstrap bootstrap |
| PF-000474 | `selfhost/bootstrap/clean_selfhost.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-004` | Verificador bootstrap clean selfhost |
| PF-000475 | `selfhost/bootstrap/dependencies.md` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-005` | Dato normativo bootstrap dependencies |
| PF-000476 | `selfhost/bootstrap/gate.template.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-006` | Dato normativo bootstrap gate.template |
| PF-000477 | `selfhost/bootstrap/generations.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-007` | Dato normativo bootstrap generations |
| PF-000478 | `selfhost/bootstrap/path-configuration.md` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-008` | Dato normativo bootstrap path configuration |
| PF-000479 | `selfhost/bootstrap/profile_selection.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-009` | Verificador bootstrap profile selection |
| PF-000480 | `selfhost/bootstrap/README.md` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-010` | Dato normativo bootstrap README |
| PF-000481 | `selfhost/bootstrap/recovery.md` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-011` | Dato normativo bootstrap recovery |
| PF-000482 | `selfhost/bootstrap/report.template.md` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-012` | Dato normativo bootstrap report.template |
| PF-000483 | `selfhost/bootstrap/run_sh44_machine.cmd` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-013` | Dato normativo bootstrap run sh44 machine |
| PF-000484 | `selfhost/bootstrap/schemas/bootstrap-lock.schema.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-014` | Dato normativo bootstrap bootstrap lock.schema |
| PF-000485 | `selfhost/bootstrap/schemas/bootstrap-profile.schema.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-015` | Dato normativo bootstrap bootstrap profile.schema |
| PF-000486 | `selfhost/bootstrap/schemas/gate.schema.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-016` | Dato normativo bootstrap gate.schema |
| PF-000487 | `selfhost/bootstrap/schemas/report.schema.json` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-017` | Dato normativo bootstrap report.schema |
| PF-000488 | `selfhost/bootstrap/SH44_PEER_README.txt` | `BOOTSTRAP_DATA` | `INDIRECTA` | `C12B-BSI-018` | Dato normativo bootstrap SH44 PEER README |
| PF-000489 | `selfhost/bootstrap/verify_advanced_toolchain.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-019` | Verificador bootstrap verify advanced toolchain |
| PF-000490 | `selfhost/bootstrap/verify_arrays_memory.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-020` | Verificador bootstrap verify arrays memory |
| PF-000491 | `selfhost/bootstrap/verify_backend_general.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-021` | Verificador bootstrap verify backend general |
| PF-000492 | `selfhost/bootstrap/verify_bootstrap_dependencies.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-022` | Verificador bootstrap verify bootstrap dependencies |
| PF-000493 | `selfhost/bootstrap/verify_bootstrap_surface.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-023` | Verificador bootstrap verify bootstrap surface |
| PF-000494 | `selfhost/bootstrap/verify_callable_graph.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-024` | Verificador bootstrap verify callable graph |
| PF-000495 | `selfhost/bootstrap/verify_closure.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-025` | Verificador bootstrap verify closure |
| PF-000496 | `selfhost/bootstrap/verify_cross_machine_determinism.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-026` | Verificador bootstrap verify cross machine determinism |
| PF-000497 | `selfhost/bootstrap/verify_data_layout.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-027` | Verificador bootstrap verify data layout |
| PF-000498 | `selfhost/bootstrap/verify_developer_experience.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-028` | Verificador bootstrap verify developer experience |
| PF-000499 | `selfhost/bootstrap/verify_distribution_final.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-029` | Verificador bootstrap verify distribution final |
| PF-000500 | `selfhost/bootstrap/verify_driver_products.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-030` | Verificador bootstrap verify driver products |
| PF-000501 | `selfhost/bootstrap/verify_eh_regions.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-031` | Verificador bootstrap verify eh regions |
| PF-000502 | `selfhost/bootstrap/verify_environment.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-032` | Verificador bootstrap verify environment |
| PF-000503 | `selfhost/bootstrap/verify_final_certification.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-033` | Verificador bootstrap verify final certification |
| PF-000504 | `selfhost/bootstrap/verify_first_generation.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-034` | Verificador bootstrap verify first generation |
| PF-000505 | `selfhost/bootstrap/verify_fixed_point.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-035` | Verificador bootstrap verify fixed point |
| PF-000506 | `selfhost/bootstrap/verify_frontend.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-036` | Verificador bootstrap verify frontend |
| PF-000507 | `selfhost/bootstrap/verify_functional_equivalence.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-037` | Verificador bootstrap verify functional equivalence |
| PF-000508 | `selfhost/bootstrap/verify_generics_aot.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-038` | Verificador bootstrap verify generics aot |
| PF-000509 | `selfhost/bootstrap/verify_image_replay_retired.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-039` | Verificador bootstrap verify image replay retired |
| PF-000510 | `selfhost/bootstrap/verify_interop_base.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-040` | Verificador bootstrap verify interop base |
| PF-000511 | `selfhost/bootstrap/verify_interop_parity.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-041` | Verificador bootstrap verify interop parity |
| PF-000512 | `selfhost/bootstrap/verify_ir.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-042` | Verificador bootstrap verify ir |
| PF-000513 | `selfhost/bootstrap/verify_llvm_backend.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-043` | Verificador bootstrap verify llvm backend |
| PF-000514 | `selfhost/bootstrap/verify_llvm_c_contract.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-044` | Verificador bootstrap verify llvm c contract |
| PF-000515 | `selfhost/bootstrap/verify_low_level_parity.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-045` | Verificador bootstrap verify low level parity |
| PF-000516 | `selfhost/bootstrap/verify_module_set.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-046` | Verificador bootstrap verify module set |
| PF-000517 | `selfhost/bootstrap/verify_native_text.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-047` | Verificador bootstrap verify native text |
| PF-000518 | `selfhost/bootstrap/verify_nsc_next_skeleton.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-048` | Verificador bootstrap verify nsc next skeleton |
| PF-000519 | `selfhost/bootstrap/verify_object_model.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-049` | Verificador bootstrap verify object model |
| PF-000520 | `selfhost/bootstrap/verify_ownership_cleanup.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-050` | Verificador bootstrap verify ownership cleanup |
| PF-000521 | `selfhost/bootstrap/verify_performance.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-051` | Verificador bootstrap verify performance |
| PF-000522 | `selfhost/bootstrap/verify_profile.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-052` | Verificador bootstrap verify profile |
| PF-000523 | `selfhost/bootstrap/verify_program_ir.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-053` | Verificador bootstrap verify program ir |
| PF-000524 | `selfhost/bootstrap/verify_project_driver.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-054` | Verificador bootstrap verify project driver |
| PF-000525 | `selfhost/bootstrap/verify_promotion_readiness.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-055` | Verificador bootstrap verify promotion readiness |
| PF-000526 | `selfhost/bootstrap/verify_repeated_fixed_point.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-056` | Verificador bootstrap verify repeated fixed point |
| PF-000527 | `selfhost/bootstrap/verify_robustness_final.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-057` | Verificador bootstrap verify robustness final |
| PF-000528 | `selfhost/bootstrap/verify_robustness.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-058` | Verificador bootstrap verify robustness |
| PF-000529 | `selfhost/bootstrap/verify_runtime_bootstrap_contract.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-059` | Verificador bootstrap verify runtime bootstrap contract |
| PF-000530 | `selfhost/bootstrap/verify_runtime_library.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-060` | Verificador bootstrap verify runtime library |
| PF-000531 | `selfhost/bootstrap/verify_self_hosted_generation.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-061` | Verificador bootstrap verify self hosted generation |
| PF-000532 | `selfhost/bootstrap/verify_semantics.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-062` | Verificador bootstrap verify semantics |
| PF-000533 | `selfhost/bootstrap/verify_source_bool_void.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-063` | Verificador bootstrap verify source bool void |
| PF-000534 | `selfhost/bootstrap/verify_source_control_flow.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-064` | Verificador bootstrap verify source control flow |
| PF-000535 | `selfhost/bootstrap/verify_source_derived_backend.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-065` | Verificador bootstrap verify source derived backend |
| PF-000536 | `selfhost/bootstrap/verify_source_expressions.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-066` | Verificador bootstrap verify source expressions |
| PF-000537 | `selfhost/bootstrap/verify_source_floating_scalars.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-067` | Verificador bootstrap verify source floating scalars |
| PF-000538 | `selfhost/bootstrap/verify_source_functions.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-068` | Verificador bootstrap verify source functions |
| PF-000539 | `selfhost/bootstrap/verify_source_graph.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-069` | Verificador bootstrap verify source graph |
| PF-000540 | `selfhost/bootstrap/verify_source_integer_widths.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-070` | Verificador bootstrap verify source integer widths |
| PF-000541 | `selfhost/bootstrap/verify_source_loop_control.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-071` | Verificador bootstrap verify source loop control |
| PF-000542 | `selfhost/bootstrap/verify_source_pointers.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-072` | Verificador bootstrap verify source pointers |
| PF-000543 | `selfhost/bootstrap/verify_specialized_code.py` | `BOOTSTRAP_VERIFIER` | `INDIRECTA` | `C12B-BSI-073` | Verificador bootstrap verify specialized code |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-BSI-001` | Dato normativo bootstrap bootstrap profile | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-002` | Dato normativo bootstrap bootstrap.config.example | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-003` | Dato normativo bootstrap bootstrap | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-004` | Verificador bootstrap clean selfhost | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-005` | Dato normativo bootstrap dependencies | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-006` | Dato normativo bootstrap gate.template | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-007` | Dato normativo bootstrap generations | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-008` | Dato normativo bootstrap path configuration | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-009` | Verificador bootstrap profile selection | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-010` | Dato normativo bootstrap README | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-011` | Dato normativo bootstrap recovery | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-012` | Dato normativo bootstrap report.template | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-013` | Dato normativo bootstrap run sh44 machine | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-014` | Dato normativo bootstrap bootstrap lock.schema | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-015` | Dato normativo bootstrap bootstrap profile.schema | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-016` | Dato normativo bootstrap gate.schema | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-017` | Dato normativo bootstrap report.schema | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-018` | Dato normativo bootstrap SH44 PEER README | `self-host` | JSON, schema, lock o política documental | parseo tipado y correspondencia exacta con productores/consumidores |
| `C12B-BSI-019` | Verificador bootstrap verify advanced toolchain | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-020` | Verificador bootstrap verify arrays memory | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-021` | Verificador bootstrap verify backend general | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-022` | Verificador bootstrap verify bootstrap dependencies | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-023` | Verificador bootstrap verify bootstrap surface | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-024` | Verificador bootstrap verify callable graph | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-025` | Verificador bootstrap verify closure | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-026` | Verificador bootstrap verify cross machine determinism | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-027` | Verificador bootstrap verify data layout | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-028` | Verificador bootstrap verify developer experience | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-029` | Verificador bootstrap verify distribution final | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-030` | Verificador bootstrap verify driver products | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-031` | Verificador bootstrap verify eh regions | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-032` | Verificador bootstrap verify environment | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-033` | Verificador bootstrap verify final certification | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-034` | Verificador bootstrap verify first generation | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-035` | Verificador bootstrap verify fixed point | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-036` | Verificador bootstrap verify frontend | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-037` | Verificador bootstrap verify functional equivalence | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-038` | Verificador bootstrap verify generics aot | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-039` | Verificador bootstrap verify image replay retired | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-040` | Verificador bootstrap verify interop base | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-041` | Verificador bootstrap verify interop parity | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-042` | Verificador bootstrap verify ir | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-043` | Verificador bootstrap verify llvm backend | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-044` | Verificador bootstrap verify llvm c contract | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-045` | Verificador bootstrap verify low level parity | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-046` | Verificador bootstrap verify module set | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-047` | Verificador bootstrap verify native text | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-048` | Verificador bootstrap verify nsc next skeleton | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-049` | Verificador bootstrap verify object model | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-050` | Verificador bootstrap verify ownership cleanup | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-051` | Verificador bootstrap verify performance | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-052` | Verificador bootstrap verify profile | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-053` | Verificador bootstrap verify program ir | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-054` | Verificador bootstrap verify project driver | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-055` | Verificador bootstrap verify promotion readiness | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-056` | Verificador bootstrap verify repeated fixed point | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-057` | Verificador bootstrap verify robustness final | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-058` | Verificador bootstrap verify robustness | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-059` | Verificador bootstrap verify runtime bootstrap contract | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-060` | Verificador bootstrap verify runtime library | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-061` | Verificador bootstrap verify self hosted generation | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-062` | Verificador bootstrap verify semantics | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-063` | Verificador bootstrap verify source bool void | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-064` | Verificador bootstrap verify source control flow | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-065` | Verificador bootstrap verify source derived backend | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-066` | Verificador bootstrap verify source expressions | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-067` | Verificador bootstrap verify source floating scalars | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-068` | Verificador bootstrap verify source functions | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-069` | Verificador bootstrap verify source graph | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-070` | Verificador bootstrap verify source integer widths | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-071` | Verificador bootstrap verify source loop control | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-072` | Verificador bootstrap verify source pointers | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |
| `C12B-BSI-073` | Verificador bootstrap verify specialized code | `self-host` | contrato versionado, productores y directorio aislado | reporte estructurado, hashes, etapas, tiempos y código de salida |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 365.
- Casos NIGHTLY/weekly: 73.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.