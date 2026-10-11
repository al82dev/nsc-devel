# Tanda de validación 184 — TP-PAR-189

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo de `TP-PAR-189` y sincroniza su
plan, inventario y manifiesto. No modifica el parser ni otras pruebas.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El parser representa `impl Trait for Type` mediante un `ClassDeclaration`
marcado `ns3-trait-impl`. `typeSyntax()` conserva el trait explícito y
`second()->typeSyntax()` conserva el tipo objetivo. El oráculo anterior exigía
una `ExtensionDeclaration` que el productor no publica.

## Corrección

La prueba verifica ahora kind, marker, trait explícito y target exactos. Se
sincronizaron plan, inventario y manifiesto.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas restantes se implementarán primero y se validarán juntas al final.
