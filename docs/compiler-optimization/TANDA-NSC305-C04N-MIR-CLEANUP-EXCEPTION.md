# Tanda NSC305-C04n — cleanupRegions + exceptionFlow (con re-enlace de punteros)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: sub-structs con punteros → índices pre-orden + re-enlace).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Serializar cleanupRegions + exceptionFlow/nonlocalExits; punteros `action`/`node` como índices pre-orden y re-enlace tras el body. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio. |
| Revisión de referencia | `development` en `fd00838e0`. |
| Archivos | `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `nodePreorderIndex`, `nodeByPreorderIndex`, `serializeMirFunctionCore`, `restoreMirFunctionCore`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `cleanupRegions` y `exceptionFlow`/`nonlocalExits` se serializan; los punteros a nodos del body van
  como índices pre-orden y se re-enlazan tras deserializar el árbol. Con esto el payload del
  `ProjectOopMirFunction` queda completo (falta sólo el consumidor).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
