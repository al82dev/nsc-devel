# Tanda 05.02 — Host.Interop

## Resultado de materialización

| Clasificación | Contratos | Casos | Flujo ordinario |
|---|---:|---:|---|
| Autocontenidos | 20 | 160 | Sí |
| `InfrastructurePending` | 28 | 224 | No |
| Total diseñado | 48 | 384 | — |

La suite ordinaria de `Host.Interop` contiene exclusivamente los 160 casos que
pueden resolverse con el compilador y el arnés aislado. Los 224 casos restantes
conservan su contrato, pero requieren infraestructura externa que esta tanda no
debe simular. No se registran como `Required`, no se clasifican como
`KnownProductDefect` y no se convierten en `PASS` ni en `SKIP`.

## Contratos autocontenidos

Los siguientes contratos aportan ocho variantes cada uno al fragmento
`tests/compiler-suite/manifest/fragments/host-interop.jsonl`:

- `CF-INTEROP-011..018` (`TF-INTEROP-081..144`)
- `CF-INTEROP-022` (`TF-INTEROP-169..176`)
- `CF-INTEROP-024..028` (`TF-INTEROP-185..224`)
- `CF-INTEROP-034..035` (`TF-INTEROP-265..280`)
- `CF-INTEROP-039` (`TF-INTEROP-305..312`)
- `CF-INTEROP-043..044` (`TF-INTEROP-337..352`)
- `CF-INTEROP-048` (`TF-INTEROP-377..384`)

## Contratos pendientes de infraestructura

Estos contratos permanecen fuera del registro y del manifiesto ordinarios con
disposición explícita `InfrastructurePending`:

- `CF-INTEROP-001..010`: proyectos, archivos, cabeceras C y toolchains de
  integración externos.
- `CF-INTEROP-019..021`: capacidades o corpus externos y recorridos de
  directorio controlados.
- `CF-INTEROP-023`: frontera del codec binario que requiere artefactos externos
  reproducibles.
- `CF-INTEROP-029..033`: producción, publicación y consumo de metadata de
  proyectos externos.
- `CF-INTEROP-036..038`: corpus histórico, corrupción y compatibilidad del
  codec fuera del arnés aislado.
- `CF-INTEROP-040..042`: publicación, historia e integridad de lectura con
  artefactos persistentes.
- `CF-INTEROP-045..047`: matrices de corrupción y validación que requieren
  fixtures especiales.

Cada contrato pendiente conserva ocho casos, para un total de 224. Su ausencia
del flujo ordinario es deliberada: ejecutarlos sin sus productores reales
falsearía el contrato probado.

## Archivos de implementación

- `tests/compiler-suite/host/modules/InteropTestSupport.h`
- `tests/compiler-suite/host/modules/InteropCHeaderModule.cpp`
- `tests/compiler-suite/host/modules/InteropSurfacesModule.cpp`
- `tests/compiler-suite/host/modules/InteropDiffSchemaModule.cpp`
- `tests/compiler-suite/host/modules/InteropMetadataModelModule.cpp`
- `tests/compiler-suite/host/modules/InteropCodecModule.cpp`
- `tests/compiler-suite/host/modules/InteropValidationMergeModule.cpp`
- `tests/compiler-suite/manifest/fragments/host-interop.jsonl`
- `tests/compiler-suite/controller/integration/run_host_interop_gate.py`

## Contrato del gate

El gate de componente debe exigir exactamente los 160 identificadores
autocontenidos, tanto en el fragmento del manifiesto como en el resultado del
ejecutor. La aparición de cualquier ID adicional, la ausencia de uno esperado o
un resultado distinto de `PASS` cierra el gate con error. Los 224 casos
`InfrastructurePending` no forman parte de ese denominador.

## Validación

Por instrucción de esta tanda no se compiló, ejecutó ni validó la suite y no se
corrió ningún gate. La integración central, la fusión del fragmento al
manifiesto canónico y la validación quedan a cargo de la fase de integración.

No se modificaron el producto, `CMakeLists.txt`, `TestRegistry.h`, `main.cpp`,
tracking, cursor ni la batería histórica.
