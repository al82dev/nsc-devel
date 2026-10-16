# Tanda NSC305-G08k — Dos pasadas: infraestructura + hallazgo de profundidad

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (dos pasadas collect→apply: infraestructura y hallazgo de la prueba de escala).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `collectTypeSpecializationDemand` + `executeTypeDemandWaves` (las dos mitades de la dos pasadas) y documentar el hallazgo de la prueba de escala. |
| Naturaleza | Infraestructura (no activada) + hallazgo. |
| Comportamiento idéntico | La materialización sigue inline; 544/544. |
| Revisión de referencia | `development` en `b4cb364f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `collectTypeSpecializationDemand`, `executeTypeDemandWaves`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `collectTypeSpecializationDemand` graba la demanda; `executeTypeDemandWaves` la ejecuta en ondas.
- La activación (processTypeSyntax → collect) se revirtió tras el hallazgo siguiente.

## 3. Hallazgo (prueba TF-FRONT-168)

Activar la dos pasadas (collect → ondas) **aplana la profundidad de instanciación**: el
`InstantiationBudget` mide la profundidad con su pila de `enter/leave`, y con ondas la pila ya no
anida, de modo que `maximumObservedDepth` baja de 16 a ~1 y la prueba de escala falla.

**Conclusión:** la dos pasadas no es un refactor aislado; requiere llevar la PROFUNDIDAD LÓGICA en la
demanda (parent depth + 1) y adaptar `InstantiationBudget` para consumirla (profundidad y detección de
ciclo por cadena de claves, no por pila de llamadas). Se retoma tras C01..C05, cuando la caché C3 ya
necesite discovery inmutable.

## 4. Evidencia

- **HECHO COMPROBADO**: con la activación revertida, `Host.Frontend` **544 PASS / 0 FAIL**.
