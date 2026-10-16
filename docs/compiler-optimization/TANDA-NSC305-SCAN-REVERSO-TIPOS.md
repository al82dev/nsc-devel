# Tanda NSC305 — Eliminación de la búsqueda reversa en `processTypeSyntax`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Micro-optimización de complejidad en la ruta de tipos (recomendada por el diseño 01 §4.9).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Eliminar el barrido lineal reverso de `specializations_` que `processTypeSyntax` hacía por `specializedName` para recuperar la clave de la especialización recién materializada. |
| Naturaleza | Refactor de complejidad no funcional (O(S) por tipo → O(1)). |
| Comportamiento idéntico | Identidad resuelta, origen de declaración, base name, diagnósticos. |
| Revisión de referencia | `development` en `9a60f25c`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `ProjectGenericModel::ensureTypeSpecialization` (parámetro opcional `outSpecializationKey`), `processTypeSyntax`. |
| Pruebas | `Host.Frontend` completo (544 casos). |

## 2. Cambio

- `ensureTypeSpecialization` acepta `std::string* outSpecializationKey` (por defecto `nullptr`) y lo
  rellena con la clave canónica en el hit de caché y en la materialización nueva.
- `processTypeSyntax` usa esa clave directamente en lugar de barrer `specializations_` en reversa.

## 3. Evidencia

- **HECHO COMPROBADO**: `nsc` compila; `Host.Frontend` **544 PASS / 0 FAIL**.
- **RESULTADO MEDIDO** (fixture de 500 structs genéricos): `root-demand-discovery` 696.096 → 691.134 µs
  (≈0,7 %). El costo dominante sigue siendo el recorrido/inferencia de `processNode` (objetivo de G04).

## 4. Siguiente acción

NSC305-G04: scopes persistentes (eliminar copias profundas de `GenericLookupScope` en `processNode`),
justificado por la medición (`root-demand-discovery` ≈ 96 % de `generics`).
