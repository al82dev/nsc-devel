# Tanda NSC305-C03a — Store del clon especializado (payload C3)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C03 (caché de especialización: persistir el clon especializado).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Persistir el clon especializado (`serializeAstNodeTree`) bajo la clave de especialización en la caché. |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, no hace nada. |
| Revisión de referencia | `development` en `c4ddbc5d6`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | (store del clon en `executeTypeSpecialization`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeSpecialization` guarda el clon (`serializeAstNodeTree`) bajo `key` cuando la caché
  está habilitada. El restore (hit C3) + re-link es la siguiente capa.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
