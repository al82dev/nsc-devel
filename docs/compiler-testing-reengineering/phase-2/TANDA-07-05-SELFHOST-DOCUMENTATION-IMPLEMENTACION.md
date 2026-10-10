# Tanda 07-05 — SelfHost.Documentation — materialización e integración

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar únicamente `CSH-DOC-001..002` y `TSH-00216..00227`.
- **Naturaleza:** infraestructura nueva de pruebas y documentación; no modifica producto ni documentos
  arquitectónicos auditados.
- **Fuentes normativas:** plan de pruebas, inventarios de Fase 1, `selfhost/nsc/README.md`,
  `selfhost/nsc/NativeSharp.Compiler.nsproj` y `CONTRADICTIONS.md`.
- **Archivos autorizados:** soporte y módulos nuevos `SelfHostDocumentation*`, fragmento vacío, gate estructural,
  registro central, entrada host, CMake y este Markdown.
- **Archivos protegidos:** producto, `SelfHost.Diagnostics`, tracking, cursor, manifiesto consolidado, batería
  histórica, `C:`, remotos y documentos canónicos de Fase 1.
- **Acciones prohibidas:** build, pruebas, gates, validación, baseline, recursión, despliegue, corrección de
  metadata canónica y fabricación de PASS.
- **Criterio de este corte:** 12/12 IDs materializados y trazados a dos contratos; infraestructura y conflicto
  de metadata visibles; cero filas ejecutables y validación pendiente.

## Denominador canónico

| Dimensión | Conteo |
|---|---:|
| Contratos | 2 |
| Casos totales | 12 |
| Capa `build-contract` | 12 |
| `InfrastructurePending` | 12 |
| Ejecutables ordinarios | 0 |
| `KnownProductDefect` | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Frontera ejecutable ausente

`CSH-DOC-001` requiere mutar de forma hermética la declaración arquitectónica, trazar cada afirmación a la
fuente/proyecto vigente y registrar cualquier desacuerdo como contradicción. `CSH-DOC-002` exige inventariar
fuentes y proyecto, aplicar matrices válidas e inválidas sobre la política bootstrap y conservar expresamente
los residuos de versión ya documentados en `CON-0011` y `CON-0014`.

El controlador general todavía no ofrece una operación self-host que posea simultáneamente README, proyecto,
árbol de fuentes, mutaciones temporales y ledger de contradicciones. Copiar esas reglas dentro de una función
que siempre pasa probaría un sustituto. Por ello los doce casos se registran como `InfrastructurePending`; su
cuerpo defensivo falla si alguien intenta invocarlos directamente.

## Conflicto de metadata preservado

El plan y `test-case-design.csv` asignan perfil `PR` a los doce IDs, mientras
`suite-case-allocation.csv` asigna `NIGHTLY`. La materialización conserva `NIGHTLY` como perfil operativo del
ejecutor planificado y registra la discrepancia completa en
`SelfHostDocumentationPlanContradictions.jsonl`. No se reescribió silenciosamente ninguna fuente canónica y el
conflicto no se confundió con un defecto del compilador.

## Artefactos

- `SelfHostDocumentationTestSupport.h`: disposición pendiente y cuerpo defensivo común.
- `SelfHostDocumentationArchitectureModule.cpp`: `TSH-00216..00221`.
- `SelfHostDocumentationPolicyModule.cpp`: `TSH-00222..00227`.
- `SelfHostDocumentationPlanContradictions.jsonl`: conflicto `PR` frente a `NIGHTLY` para los 12 IDs.
- `manifest/cases.jsonl`: conserva cero filas de `SelfHost.Documentation` porque todos los casos son pendientes.
- `run_selfhost_documentation_gate.py`: comprobación futura del denominador, linaje, disposición y conflicto.

Los recorridos son iterativos y de tamaño lineal respecto al inventario; no existe recursión.

## Estado

Los dos registros, sus módulos y el gate estructural están conectados en `TestRegistry.h`, `main.cpp` y CMake.
El fragmento vacío se retiró; el gate exige que permanezca ausente y que el manifiesto central conserve cero
filas del componente. No se modificaron tracking ni cursor. No se ejecutaron build, pruebas, gates o runners.
Conclusión exacta:

`MATERIALIZACIÓN E INTEGRACIÓN ESTÁTICA COMPLETAS: 12/12 INFRASTRUCTUREPENDING; VALIDACIÓN PENDIENTE`.
