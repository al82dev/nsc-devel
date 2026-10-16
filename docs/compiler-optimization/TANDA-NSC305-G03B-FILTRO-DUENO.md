# Tanda NSC305-G03b — Resultado compartido de resolución de métodos (filtro de dueño)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G03b (reducir reconstrucción redundante de la escalera en la resolución de métodos).

## 1. SCOPE LOCK de la tanda

| Campo | Valor |
|---|---|
| Objetivo | Eliminar el `templatesNamedBy` redundante que `collectMethodTemplates` hacía dentro de `processGenericCall` y `collectCallDependency` (ambos ya habían calculado el rung ganador). |
| Naturaleza | Refactor de complejidad no funcional (una consulta menos por sitio). |
| Comportamiento idéntico | Overloads, primer rung ganador, owner ambiguity, controlled-specialization, diagnósticos. |
| Revisión de referencia | `development` en `8d48cb33`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | nuevo `collectMethodTemplatesFromNamed`; `collectMethodTemplates` lo delega; `processGenericCall` y `collectCallDependency` reusan el rung ganador. |
| Pruebas | `Host.Frontend` completo (544 casos) sin modificaciones. |
| Cierre | Filtro de dueño extraído; llamada redundante eliminada; 544/544; expediente + commit local aislado. |

## 2. Cambio

- Se extrae el filtro de «dueño único» de `collectMethodTemplates` a `collectMethodTemplatesFromNamed`
  (puro, const), que toma el vector del rung ganador ya calculado.
- `processGenericCall` y `collectCallDependency` reusan `templatesNamedBy(...)` (ya computado para el
  diagnóstico de ambigüedad) y aplican el filtro sin reconstruir la escalera.

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Limitación honesta

El gate completo de G03 («una llamada no construye más de una escalera») queda **parcialmente** cubierto:
`resolveMethodTemplate` aún construye la escalera para sus intentos (aridad exacta/sin-filtro + camino sin
dueño). Tras G03a la escalera es un vector local barato y el barrido pasó de O(T) a O(C); la reducción
restante es de bajo valor y de riesgo no trivial (interacción ambigüedad/camino-sin-dueño), por lo que se
pospone hasta que la telemetría P01 indique que la reconstrucción de la escalera es un cuello medible.

## 5. Siguiente acción

NSC305-G04: scopes persistentes (eliminar copias profundas de `GenericLookupScope` en `processNode`).
