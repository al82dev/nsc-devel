# Tanda NSC305-C05a — `TemplateParseCache::prune` (límite de caché)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C05 (política: evicción por publicación más antigua en la caché de plantillas).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `TemplateParseCache::prune` (evicción oldest-first hasta el presupuesto). |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, no hace nada. |
| Revisión de referencia | `development` en `9d5efbdd6`. |
| Archivos | `include/nativesharp/Frontend/TemplateParseCache.h`, `src/Frontend/TemplateParseCache.cpp`. |
| Símbolos | `TemplateParseCache::prune`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `prune` recorre las entradas `.meta`, suma tamaños y evacúa por publicación más antigua hasta
  quedar bajo `maximumBytes`, siguiendo la convención de `CompilationArtifactCache`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
