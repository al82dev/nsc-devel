# Segunda auditoría consolidada — Tanda 12A, compilador self-host

## Universo contrastado

La segunda pasada partió de `inventory/production-files.csv`, no de los documentos redactados. Volvió a
enumerar `selfhost/nsc/**`, contrastó hashes y tamaños, exigió una disposición para cada archivo y recorrió las
referencias archivo → declaración → contrato → caso → documento.

| Gate | Resultado |
|---|---:|
| Archivos inventariados / leídos / auditados | 29 / 29 / 29 |
| Fuentes `.ns` / proyecto / README | 27 / 1 / 1 |
| Componentes | 16 |
| Declaraciones físicas | 874 |
| Contratos | 138 |
| Casos diseñados | 1,027 |
| Casos PR / NIGHTLY | 900 / 127 |
| Documentos / test plans / auditorías por componente | 16 / 16 / 16 |
| Archivos sin disposición | 0 |
| Declaraciones self-host sin contrato | 0 |
| Contratos self-host sin casos | 0 |
| Referencias self-host rotas | 0 |
| Omitidos / desconocidos | 0 / 0 |

## Comprobaciones independientes

- La suma por componente reproduce 29 archivos, 874 declaraciones, 138 contratos y 1,027 casos.
- Los 29 hashes provienen del manifiesto canónico; la auditoría no volvió a definir el universo desde globs de
  pruebas ni desde nombres históricos.
- Las 874 declaraciones tienen un contrato primario. Los dos archivos sin declaraciones ejecutables tienen
  contratos explícitos de proyecto y documentación.
- Cada contrato posee válido mínimo, válido representativo, prerrequisito ausente, estructura inválida, bordes
  y variantes. Determinismo, transición parcial y escala sólo se añadieron cuando el contrato las posee.
- `transversal-contracts.csv` y `error-taxonomy.csv` contienen exactamente 1,008 filas: una por cada contrato
  host más self-host. La tabla de máquinas de estado contiene 602 asociaciones no vacías.
- Los seis archivos modificados antes de esta fase conservan exactamente sus hashes protegidos.
- No existe cambio fuera de `docs/compiler-testing-reengineering/**` atribuible a la Tanda 12A.

## Contradicciones conservadas

La pasada registró, sin corregir producto, los residuos de edición/metadata, los recorridos recursivos y el
comentario histórico de `LLVM-C.dll` como `CON-0011` a `CON-0013`.

## Hallazgo previo para Tanda 13

`SYM-005151` ya estaba duplicado entre `Host.Driver` y `Host.LanguageService` antes de esta tanda. Ningún símbolo
self-host reutiliza un ID existente. La Tanda 13 debe reparar la identidad histórica y todas sus referencias antes
del gate final global; el hallazgo no invalida la cobertura del universo self-host, pero sí impide afirmar todavía
que el inventario global de Fase 1 está cerrado.

## Dictamen

La Tanda 12A queda `LISTA`: los 29 archivos self-host están semánticamente leídos, clasificados, consolidados,
contrastados y auditados. Los 16 componentes son documentalmente `RECONSTRUIBLE`. No se afirma suite implementada,
ejecución, cobertura, equivalencia histórica ni cierre de la Fase 1.
