# Tanda NSC305-G03b — Reutilizar la ladder exacta en `resolveMethodTemplate`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G03 (cierre parcial: eliminar la resolución exacta redundante por llamada).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Evitar que `collectCallDependency` reconstruya la ladder exacta (`templatesNamedBy(arity, argumentCount)`) que ya calculó, pasándola a `resolveMethodTemplate`. |
| Naturaleza | Refactor no funcional (elimina un barrido redundante; misma política de ambigüedad/fallback). |
| Comportamiento idéntico | Resolución de métodos, ambigüedad, fallback sin dueño. |
| Revisión de referencia | `development` en `4e752ea9`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `resolveMethodTemplate` (+parámetro `named` opcional); `collectCallDependency`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `resolveMethodTemplate` acepta `named` (opcional, `nullptr`): cuando se le pasa, el primer intento
  (cuenta de argumentos exacta) reutiliza ese vector en lugar de reconstruir la ladder.
- `collectCallDependency` pasa el `named` ya computado, eliminando 1 de las ~5 resoluciones por sitio.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Nota

El objetivo completo de G03 ("una sola ladder por llamada") requeriría compartir la ladder también
entre los intentos de aridad relajada; es de valor micro y se deja documentado. G02-triestado
(`resolveTypeTemplate` devolviendo `Missing/Unique/Ambiguous`) no tiene consumidor de diagnóstico en
esta fase (los llamadores tratan `nullptr` uniformemente), por lo que se difiere por austeridad.
