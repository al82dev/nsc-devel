# Índice — Reingeniería de pruebas de NSC

## Orden de lectura

1. [SCOPE-LOCK.md](SCOPE-LOCK.md)
2. [PHASE-1-MASTER-PLAN.md](PHASE-1-MASTER-PLAN.md)
3. [TRACKING.md](TRACKING.md)
4. [inventory/README.md](inventory/README.md)
5. [architecture/COMPONENT-GRAPH.md](architecture/COMPONENT-GRAPH.md)
6. Documentos de componentes en `components/`
7. Planes de prueba en `test-plans/`
8. [TRANSVERSAL-CONTRACTS.md](TRANSVERSAL-CONTRACTS.md)
9. [ERROR-TAXONOMY.md](ERROR-TAXONOMY.md)
10. [STATE-MACHINES.md](STATE-MACHINES.md)
11. Informes de auditoría independiente en `audits/`

## Gate de integridad previo a la arquitectura

- [Hallazgo de cobertura antes de Tanda 12](audits/TANDA-12-SCOPE-COMPLETENESS-GATE.md): distingue la matriz
  host ya cerrada de los componentes self-host y superficies restantes que todavía requieren tratamiento.

## Componentes consolidados

- [Host.Basic](components/HOST-BASIC.md)
  - [Test plan](test-plans/HOST-BASIC-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-BASIC-AUDIT.md)
- [Host.Source](components/HOST-SOURCE.md)
  - [Test plan](test-plans/HOST-SOURCE-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-SOURCE-AUDIT.md)
- [Host.Diagnostics](components/HOST-DIAGNOSTICS.md)
  - [Test plan](test-plans/HOST-DIAGNOSTICS-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-DIAGNOSTICS-AUDIT.md)
- [Host.Lexer](components/HOST-LEXER.md)
  - [Test plan](test-plans/HOST-LEXER-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-04-HOST-LEXER-AUDIT.md)
- [Host.Parser](components/HOST-PARSER.md)
  - [Test plan](test-plans/HOST-PARSER-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-04-HOST-PARSER-AUDIT.md)
- [Host.AST](components/HOST-AST.md)
  - [Test plan](test-plans/HOST-AST-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-04-HOST-AST-AUDIT.md)
- [Host.Language](components/HOST-LANGUAGE.md)
  - [Test plan](test-plans/HOST-LANGUAGE-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-05-HOST-LANGUAGE-AUDIT.md)
- [Host.Sema](components/HOST-SEMA.md)
  - [Test plan](test-plans/HOST-SEMA-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-05-HOST-SEMA-AUDIT.md)
- [Host.Frontend](components/HOST-FRONTEND.md)
  - [Test plan](test-plans/HOST-FRONTEND-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-06-HOST-FRONTEND-AUDIT.md)
- [Host.HIR](components/HOST-HIR.md)
  - [Test plan](test-plans/HOST-HIR-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-HIR-AUDIT.md)
- [Host.MIR](components/HOST-MIR.md)
  - [Test plan](test-plans/HOST-MIR-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-MIR-AUDIT.md)
- [Host.CFG](components/HOST-CFG.md)
  - [Test plan](test-plans/HOST-CFG-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-CFG-AUDIT.md)
- [Host.SSA](components/HOST-SSA.md)
  - [Test plan](test-plans/HOST-SSA-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-SSA-AUDIT.md)
- [Host.Dominance](components/HOST-DOMINANCE.md)
  - [Test plan](test-plans/HOST-DOMINANCE-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-DOMINANCE-AUDIT.md)
- [Host.Passes](components/HOST-PASSES.md)
  - [Test plan](test-plans/HOST-PASSES-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-07-HOST-PASSES-AUDIT.md)
- [Host.Target](components/HOST-TARGET.md)
  - [Test plan](test-plans/HOST-TARGET-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-08-HOST-TARGET-AUDIT.md)
- [Host.LLVM](components/HOST-LLVM.md)
  - [Test plan](test-plans/HOST-LLVM-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-08-HOST-LLVM-AUDIT.md)
- [Host.Interop](components/HOST-INTEROP.md)
  - [Test plan](test-plans/HOST-INTEROP-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-09-HOST-INTEROP-AUDIT.md)
- [Host.Linker](components/HOST-LINKER.md)
  - [Test plan](test-plans/HOST-LINKER-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-09-HOST-LINKER-AUDIT.md)
- [Host.Runtime](components/HOST-RUNTIME.md)
  - [Test plan](test-plans/HOST-RUNTIME-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-09-HOST-RUNTIME-AUDIT.md)
- [Host.Compilation](components/HOST-COMPILATION.md)
  - [Test plan](test-plans/HOST-COMPILATION-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-10-HOST-COMPILATION-AUDIT.md)
- [Host.Driver](components/HOST-DRIVER.md)
  - [Test plan](test-plans/HOST-DRIVER-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-10-HOST-DRIVER-AUDIT.md)
- [Host.LanguageService](components/HOST-LANGUAGE-SERVICE.md)
  - [Test plan](test-plans/HOST-LANGUAGE-SERVICE-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-10-HOST-LANGUAGE-SERVICE-AUDIT.md)
- [Host.nsc](components/HOST-NSC.md)
  - [Test plan](test-plans/HOST-NSC-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-10-HOST-NSC-AUDIT.md)

## Artefactos mecánicos

- `inventory/production-files.csv` y `.json`: manifiesto canónico de producción y soporte.
- `inventory/components.csv`: agregación de 57 componentes documentales.
- `inventory/historical-tests-freeze.csv`: batería histórica protegida por huellas.
- `inventory/document-coverage.csv`: trazabilidad entre archivo, símbolo, contrato y sección.
- `inventory/symbols.csv`: símbolos, responsabilidades, contratos y capa futura de prueba.
- `inventory/contracts.csv`: contratos estables derivados de la implementación vigente.
- `inventory/test-case-design.csv`: casos lógicos diseñados, sin implementación ejecutable.
- `inventory/transversal-contracts.csv`: clasificación 1:1 de los 870 contratos en seis ejes transversales.
- `inventory/error-taxonomy.csv`: error, recuperación y oráculo por cada contrato.
- `inventory/state-machines.csv`: doce máquinas contractuales normalizadas.
- `inventory/state-machine-contract-map.csv`: relación completa contrato-máquina; conserva los contratos stateless
  en la matriz transversal.
- `inventory/claims.csv`: catálogo de afirmaciones y procedencia.
- `CURSOR.json`: punto exacto de reanudación.

## Registros transversales

- [CONTRADICTIONS.md](CONTRADICTIONS.md)
- [BLOCKERS.md](BLOCKERS.md)
- [RECONSTRUCTIBILITY.md](RECONSTRUCTIBILITY.md)
- [Método de clasificación transversal](evidence/TANDA-11-TRANSVERSAL-METHOD.md)
- [Segunda auditoría de Tanda 11](audits/TANDA-11-TRANSVERSAL-AUDIT.md)

## Leyenda de afirmaciones

Los documentos distinguen `HECHO COMPROBADO`, `INFERENCIA`, `DISEÑO PROPUESTO`, `IMPLEMENTACIÓN VIGENTE` y
`NO VERIFICADO`. Los planes futuros no se presentan como pruebas implementadas.

<!-- BEGIN TANDA-12A-SELFHOST-INDEX -->
## Compilador self-host

- Método y universo: `evidence/TANDA-12A-SELFHOST-METHOD.md`.
- Inventario por archivo: `inventory/selfhost-file-audit.csv`.
- Auditoría consolidada: `audits/TANDA-12A-SELFHOST-AUDIT.md`.
- `SelfHost.Basic`: `components/SELFHOST-BASIC.md`; plan: `test-plans/SELFHOST-BASIC-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-BASIC-AUDIT.md`.
- `SelfHost.Bootstrap`: `components/SELFHOST-BOOTSTRAP.md`; plan: `test-plans/SELFHOST-BOOTSTRAP-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-BOOTSTRAP-AUDIT.md`.
- `SelfHost.CodeGen`: `components/SELFHOST-CODEGEN.md`; plan: `test-plans/SELFHOST-CODEGEN-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-CODEGEN-AUDIT.md`.
- `SelfHost.Diagnostics`: `components/SELFHOST-DIAGNOSTICS.md`; plan: `test-plans/SELFHOST-DIAGNOSTICS-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-DIAGNOSTICS-AUDIT.md`.
- `SelfHost.Documentation`: `components/SELFHOST-DOCUMENTATION.md`; plan: `test-plans/SELFHOST-DOCUMENTATION-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-DOCUMENTATION-AUDIT.md`.
- `SelfHost.Driver`: `components/SELFHOST-DRIVER.md`; plan: `test-plans/SELFHOST-DRIVER-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-DRIVER-AUDIT.md`.
- `SelfHost.IR`: `components/SELFHOST-IR.md`; plan: `test-plans/SELFHOST-IR-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-IR-AUDIT.md`.
- `SelfHost.Lexer`: `components/SELFHOST-LEXER.md`; plan: `test-plans/SELFHOST-LEXER-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-LEXER-AUDIT.md`.
- `SelfHost.Parser`: `components/SELFHOST-PARSER.md`; plan: `test-plans/SELFHOST-PARSER-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-PARSER-AUDIT.md`.
- `SelfHost.Project`: `components/SELFHOST-PROJECT.md`; plan: `test-plans/SELFHOST-PROJECT-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-PROJECT-AUDIT.md`.
- `SelfHost.Projects`: `components/SELFHOST-PROJECTS.md`; plan: `test-plans/SELFHOST-PROJECTS-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-PROJECTS-AUDIT.md`.
- `SelfHost.Sema`: `components/SELFHOST-SEMA.md`; plan: `test-plans/SELFHOST-SEMA-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-SEMA-AUDIT.md`.
- `SelfHost.Source`: `components/SELFHOST-SOURCE.md`; plan: `test-plans/SELFHOST-SOURCE-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-SOURCE-AUDIT.md`.
- `SelfHost.Symbols`: `components/SELFHOST-SYMBOLS.md`; plan: `test-plans/SELFHOST-SYMBOLS-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-SYMBOLS-AUDIT.md`.
- `SelfHost.Syntax`: `components/SELFHOST-SYNTAX.md`; plan: `test-plans/SELFHOST-SYNTAX-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-SYNTAX-AUDIT.md`.
- `SelfHost.Types`: `components/SELFHOST-TYPES.md`; plan: `test-plans/SELFHOST-TYPES-TEST-PLAN.md`; auditoría: `audits/TANDA-12A-SELFHOST-TYPES-AUDIT.md`.
<!-- END TANDA-12A-SELFHOST-INDEX -->
