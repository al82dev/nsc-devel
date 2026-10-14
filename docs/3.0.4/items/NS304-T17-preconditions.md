# NS304-T17 — Precondiciones de conformidad y cierre

- **Estado**: NO INICIADO (las precondiciones duras no se cumplen)
- **Rama**: development

## Verificación de precondiciones (ronda 163)

Antes de F01 deben cumplirse simultáneamente:

| precondición | estado |
|---|---|
| T03–T16 cerradas con evidencia por ítem | ❌ (T05–T07 núcleos ✅; T08–T16 mayormente auditadas, no cerradas) |
| tracking sin huecos secuenciales ni doble conteo | ❌ (residuales documentados) |
| cero `DeferredTo4`/`PlannedFor304` en el alcance | ❌ (surfaces pendientes de Alberto) |
| host/self-host parity | ❌ (T16-F06 auditada, no ejecutada) |
| metadata/ABI/debug versionados | ❌ (T16-F01/F05 auditadas, no cerradas) |
| repositorio limpio y commits de una sola hipótesis | ✅ |
| manifest de Builder canónico y sin casos no registrados | ❌ (T16-F08 auditada) |

**Conclusión**: una precondición falla ⇒ **T17 no empieza**. No se abre la matriz C# 14 (F01) hasta
cerrar las tandas causales T03–T16 con evidencia.

## Nota

T17 es certificación, no desarrollo. Cada hueco reabre su tanda causal T03–T16; no se ajustan cifras
ni se falsea el cierre. La corrida externa limpia (5,786/5,786) certifica la base anterior, no los
cambios T03–T16.
