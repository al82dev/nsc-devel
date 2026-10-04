# Baseline 2026-10-04: transferencia del resultado opaco sintético

## Problema

Los métodos sintéticos `FromRepresentation` construían un valor opaco local y lo devolvían como
una copia ordinaria. Desde que los valores estructurales son move-only salvo declaración `copy`, el
frontend rechazaba su propio AST generado con NS3020.

## Cambios

- La síntesis directa del parser envuelve `__nsc_opaque_result` en `MoveExpression` sólo para un
  tipo opaco move-only; un `copy opaque type` conserva el retorno ordinario.
- La reconstrucción de resultados opacos precompilados aplica exactamente la misma decisión.
- Los casos que ejercitan semántica de copia declaran explícitamente `copy opaque type`; los tipos
  opacos ordinarios conservan el valor predeterminado move-only.

## Archivos

- `src/Parser/Parser.cpp`
- `include/nativesharp/Parser/Parser.h`
- `src/Frontend/ProjectOpaqueResultModel.cpp`
- `tests/unit/NativeSharp3T04TypeAliasTests.cpp`

## Mejora

Los tipos opacos vuelven a compilar en el módulo productor y a cruzar NSMETA sin concederles
copiabilidad implícita. La corrección respeta el contrato move-only en vez de abrir una excepción
semántica para código generado.
