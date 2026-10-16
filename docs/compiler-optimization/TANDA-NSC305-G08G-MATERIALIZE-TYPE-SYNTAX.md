# Tanda NSC305-G08g — Extraer `materializeTypeSyntax` (reescritura de tipo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (separar la reescritura de tipo de la resolución).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la materialización + reescritura de un tipo cerrado resuelto (ensureTypeSpecialization + setBaseName/clearTypeArguments) de `processTypeSyntax` a `materializeTypeSyntax`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Materialización y reescritura de tipo. |
| Revisión de referencia | `development` en `eff9011f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `materializeTypeSyntax`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `materializeTypeSyntax` materializa el tipo cerrado y reescribe el spelling.
- `processTypeSyntax` queda con la resolución/aridad/normalización (análisis) + delega la reescritura;
  separación discovery/rewrite más clara.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
