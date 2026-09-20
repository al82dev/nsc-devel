# NativeSharp 2.0 self-hosting — tanda 30: IR validado de programa completo

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH28` |
| Rama | `development-claude` |
| Fecha | 2026-09-20 |
| Commit de entrada | `d8bfb54a3` |
| Commit de cierre | este commit (`selfhost(tanda-30): require validated whole-program IR`) |

## Resultado y diseño

`ValidatedIrProgram` es ahora el único ticket de entrada a `LlvmBackend.EmitExecutableObject`.
`IrPipeline.Prepare` construye HIR y MIR, y no marca el programa válido hasta aprobar:

1. `HirVerifier`;
2. `FlowVerifier`;
3. `OwnershipCleanupVerifier`;
4. dominancia para todas las funciones;
5. la misma tabla semántica que originó HIR/MIR.

Se retiró la entrada pública del backend que aceptaba directamente `SyntaxTree + SemanticModel`.
El backend todavía consulta los source maps conservados dentro del objeto IR validado para el
lowering de la rebanada actual; ya no existe una ruta del driver que pueda saltarse la validación.

`MirBuilder` también dejó de inspeccionar sólo hijos directos del compilation unit. Su nuevo
`CollectFunctions` recorre el árbol con un bitmap de visitados y descubre funciones y métodos bajo
namespaces/tipos sin duplicar nodos. Esto corrigió un defecto real: el proyecto SH27 ejecutaba,
pero sus dumps MIR/CFG/SSA quedaban vacíos porque sus funciones estaban en un namespace.

## Superficie y archivos

- `IrPipeline.Prepare(SyntaxTree, SemanticModel) -> ValidatedIrProgram`;
- `LlvmBackend.EmitExecutableObject(ValidatedIrProgram, ...)`;
- `Program.DumpProjectIr` y `--dump-project-ir <nsproj> hir|mir|cfg|dominance|ssa`;
- `selfhost/contracts/program-ir.contract.json`;
- `selfhost/bootstrap/verify_program_ir.py`;
- `cmake/VerifyNativeSharpProgramIr.cmake`;
- registro CTest `nsc.selfhost.program-ir`.

## Gate SH28

La misma fixture multimódulo produce cinco superficies (HIR, MIR, CFG, dominancia y SSA) con cada
compilador candidato O0–O3. Cada superficie es byte a byte idéntica entre perfiles. MIR, CFG,
dominancia y SSA contienen exactamente las funciones relevantes `Sum6`, `Twice` y `Main`.

Resultado dirigido: **PASSED**, 11.78 s. El gate depende de SH27, que conserva la ejecución real
O0–O3 con LLD y NSLink.

- [x] HIR/MIR/CFG/dominancia/SSA del programa completo.
- [x] Entrada cruda del backend eliminada.
- [x] Un único objeto IR validado admite codegen.
- [x] Modelo determinista común O0–O3.
- [ ] Baseline completo acumulado al terminar las tandas restantes.
