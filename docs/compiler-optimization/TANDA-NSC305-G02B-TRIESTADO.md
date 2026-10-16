# Tanda NSC305-G02b — `resolveTypeTemplate` devuelve triestado Missing/Unique/Ambiguous

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G02 (cierre: triestado de resolución de tipo).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Cambiar `resolveTypeTemplate` para devolver `TypeResolution{kind, state}` con `Missing`/`Unique`/`Ambiguous` en lugar de `TemplateState*` (que colapsaba Missing y Ambiguous en `nullptr`). |
| Naturaleza | Refactor no funcional (los llamadores tratan "no Unique" uniformemente; la distinción queda disponible). |
| Comportamiento idéntico | Resolución de tipos, materialización y diagnóstico. |
| Revisión de referencia | `development` en `db64154f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `TypeResolutionKind`, `TypeResolution`, `resolveTypeTemplate`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `TypeResolutionKind` (`Missing`/`Unique`/`Ambiguous`) y `TypeResolution{kind, state}`.
- `resolveTypeTemplate` distingue: bucket-miss/empty → `Missing`; segundo candidato → `Ambiguous`;
  un candidato → `Unique`.
- Los 3 llamadores usan `resolution.kind == Unique` (en vez de `!= nullptr`); conducta idéntica.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
