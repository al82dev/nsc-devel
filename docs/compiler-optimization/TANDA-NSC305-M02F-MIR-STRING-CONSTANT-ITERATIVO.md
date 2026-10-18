# Tanda NSC 3.0.5 M02F — evaluación iterativa de constantes string

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02F migra exclusivamente la autorrecursión de
`MirConstantEvaluator::stringValue` al atravesar conversiones `builtin:string`. La hoja comienza
después del cierre M02E y de la auditoría física del siguiente módulo MIR. No modifica integer,
floating ni boolean constant evaluation; tampoco cambia HIR, MIR, sintaxis, diagnósticos, ABI,
NSMETA, LLVM ni layouts.

## Paridad algorítmica

La implementación histórica resolvía inmediatamente un `StringLiteral`, consultaba un
`StaticFieldValue` constante mediante `knownStaticField` o, para una conversión cuyo destino era
`builtin:string` y tenía hijo, volvía a evaluar sólo el primer hijo. Todo otro nodo devolvía
`nullopt`.

La ruta nueva mantiene un único cursor no propietario. En cada iteración conserva esas mismas
fronteras y resultados; sólo cuando encuentra la conversión string válida avanza al primer hijo.
Un hijo nulo termina con `nullopt`, igual que la llamada histórica. No introduce memoización,
reordenamiento, deduplicación ni evaluación de ramas nuevas.

## Contrafactual e infraestructura

El focus `mir-string-constant-depth` construye un `StringLiteral` con valor `deep-string`, lo
envuelve en 32,768 nodos `Conversion` de tipo `builtin:string`, llama directamente la función
productiva y exige el valor exacto. La destrucción del árbol usa el helper iterativo existente.

La prueba se integra en el target y `add_test` ya existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, tanto por selector como por la ruta
default. No se agrega fuente, target, catálogo, manifiesto ni caso Builder. `MirConstantEvaluator.cpp`
ya está registrado una vez en CMake raíz y una vez en compiler-suite.

## Validación

El cuerpo productivo y el contrafactual quedan por debajo de 60 statements ejecutables. Sobre el
worktree compartido que incorporaba D03 y R02A coherentes pasaron:

- build `nsc`, incluido el contrato de host estático;
- build `nsc_project_oop_lowering_tests`;
- focus `mir-string-constant-depth`: `PASS: 1 checks`;
- CTest `nsc.project-oop-lowering`: `1/1 PASS`;
- build `nsc_cc1_correctness_tests`;
- CTest real `nsc.cc1.correctness.cc1-c53-const-string`: `1/1 PASS`, con literal, alias y string
  vacío en O0/O2;
- `validate_source_registration.py`: exit `0`.

La evidencia persistente está en:

- `A:\temp\m02f-build-nsc-d03-fix.log`;
- `A:\temp\m02f-build-lowering.log`;
- `A:\temp\m02f-focus-string-constant-depth.log`;
- `A:\temp\m02f-ctest-project-oop-lowering.log`;
- `A:\temp\m02f-build-cc1-correctness.log`;
- `A:\temp\m02f-ctest-cc1-c53-const-string.log`;
- `A:\temp\m02f-source-registration.log`.

No se ejecutó baseline ni suite completa.
