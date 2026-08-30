# TEST MATRIX — NativeSharp Stage 51 (cierre)

## Identity

```text
nsc 0.1.0-dev51 (NativeSharp 0.1, stage 51)
```

## Automated matrices (este sandbox, GCC)

| Gate | Result |
|---|---:|
| `nsc.stage51.complete-string` (superficie String original) | 23/23 PASS |
| `nsc.stage51.enum-oop-support` (fix de enums, nuevo esta ronda) | 24/24 PASS |
| `nsc.driver` (incluye el fix de "Stage 49" en `--help`) | 166/166 PASS |
| `nsc.stage49.raii-move` (substring de diagnóstico actualizado) | 80/80 PASS |
| `nsc.stage35.recovery` (calificador `nuw` de LLVM 20) | 80/80 PASS |
| `nsc.llvm-lowering` (mismo fix de `nuw`) | 37/37 PASS |
| Full CTest, árbol completo | 202/203 PASS (99%) |

Única falla: `nsc.stage50_10.ownership-examples-documentation` — gap de contenido
(README histórico inexistente en este árbol), no un bug de lógica. Ver
`STAGE51_CHECKPOINT.md` y `PACKAGE_VERIFICATION_STAGE51.md` para el detalle.

## Cobertura enfocada — fix de enums (`nsc.stage51.enum-oop-support`, 24 checks)

- registro de miembros de enum (`ProjectMemberModel::collectEnumMembers`);
- valores por auto-incremento (0, 1, 2, ...) y por literal explícito (incl. negativo);
- rechazo de nombre de miembro duplicado (regresión guardada);
- rechazo de initializer no-literal / fuera de rango (regresión guardada);
- acceso calificado (`Mode.Second`) resuelve a valor real;
- igualdad entre valores de enum;
- paso de valor de enum como parámetro de función;
- cast explícito `(int)` sobre un valor de enum, verificado en el LLVM emitido
  (`constant i32` real, no un placeholder);
- emisión de objeto COFF real desde un programa que usa el enum de punta a punta.

## Cobertura enfocada — superficie String completada (69 familias, 90 overloads)

Familias agregadas o completadas en esta ronda de cierre respecto del paquete previamente
entregado (47 familias / 61 overloads):

`Split`, `RSplit`, `SplitLines`, `Partition`, `RPartition`, `Count`, `RemovePrefix`,
`RemoveSuffix`, `Compare`, `CompareOrdinal`, `CompareTo`, `Clone`, `ToLowerInvariant`,
`ToUpperInvariant`, `Format`, `IsDecimal`, `IsNumeric`, `IsIdentifier`, `IsPrintable`,
`IsTitle`, `ExpandTabs`, `ReplaceLineEndings`, más los overloads reales con
`StringComparison` de `Equals`/`IndexOf`/`Contains`/`StartsWith`/`EndsWith`.

Verificado con pipeline completo sobre `examples/project_stage51/Main.ns`
(`--check-project-method-bodies` → `--check-project-oop-hir` → `--check-project-oop-mir` →
`--verify-project-oop-llvm` → `--emit-project-oop-obj` → `--emit-project-oop-exe`), y con
un port línea-por-línea a Python de los algoritmos más delicados (Split/RSplit/SplitLines/
ExpandTabs/ReplaceLineEndings/Format) contra los mismos valores de test, dado que este
sandbox no puede ejecutar el `.exe` PE32+ resultante.

## Regresiones resueltas esta ronda (causa raíz verificada, no asumida)

| Test | Causa raíz | Fix |
|---|---|---|
| `nsc.version` | literal `dev50`/`stage 50` hardcodeado | deriva de `kLanguageVersion`/`kCompilerStage` reales |
| `nsc.stage50_10` (parcial) | mismo problema, duplicado | mismo fix; el check de contenido README queda como gap honesto, no forzado |
| `nsc.stage49.raii-move` | substring de diagnóstico obsoleto tras agregar `raii string` | actualizado a incluir "...or native string" |
| `nsc.stage35.recovery` | LLVM 20 agrega `nuw` a los GEP; substring literal completo no matcheaba | check partido en dos substrings independientes |
| `nsc.llvm-lowering` | mismo motivo que el anterior | mismo fix |

Confirmado contra la baseline real: 2 de estas 5 fallas ya reproducían en un rebuild desde
cero del propio `NativeSharp_Stage50_FULL.zip` certificado, en este mismo sandbox — no eran
"preexistentes por asunción", se verificaron ejecutando la suite completa de ese árbol.
