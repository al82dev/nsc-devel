# Tanda NSC305-G08i — Extraer `processNodeOnce` (cuerpo por nodo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (separar el cuerpo por nodo del traversal iterativo).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer el cuerpo por nodo de `processNode` (match/template/constraint/tipo/llamada/inferencia) a `processNodeOnce`, dejando `processNode` como el collector iterativo. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Traversal, materialización e inferencia. |
| Revisión de referencia | `development` en `e1d9fde7`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `processNodeOnce`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `processNodeOnce` contiene el cuerpo por nodo y devuelve `false` cuando hay que saltar hijos
  (alias directive / template abierto).
- `processNode` queda como: `prepareNodeScope` → `processNodeOnce` → recursión de hijos (pila).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
