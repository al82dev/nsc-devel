# Tanda NSC305-G06a — Eliminar recursión en lambda de expansión de packs

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (primer paso: eliminar una recursión escondida en `std::function`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Sustituir el `std::function` recursivo que recogía nombres de packs en `cloneExpandedTypeArgument` por un recorrido iterativo con pila explícita. |
| Naturaleza | Refactor no funcional (elimina recursión escondida en lambda; mismo orden de recogida). |
| Comportamiento idéntico | Nombres de pack recogidos, orden, errores de pack. |
| Revisión de referencia | `development` en `55ecc63e`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneExpandedTypeArgument` (lambda `collect`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | Lambda recursiva eliminada; 544/544; expediente + commit local aislado. |

## 2. Cambio

- El `std::function<void(const ast::TypeSyntax*)> collect` (recursivo sobre `typeArguments`) se
  reemplaza por una pila `collectStack` de `TypeSyntax*`. Orden preservado (preorden).

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Pendiente (misma tanda)

Quedan otras recursiones escondidas en lambdas en `ProjectGenericModel.cpp` (`collectOrdinaryReturns`
~684, `collect` en `mergeTraitImplementations` ~959, `visitProjection` ~1405, `markRecursiveCalls` ~2220,
`structuralTypeKey` ~3344, `patternSpecificity` ~3786, `containsOpenParameter` ~3895, `visit` ~5794).
Se convertirán en pasos siguientes de G06.
