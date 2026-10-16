# Tanda NSC305-G08h — Extraer `prepareNodeScope` (construcción de scope por nodo)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G08 (separar la construcción del scope léxico de `processNode`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la construcción del scope léxico por nodo (namespace, using/aliases, return esperado, snapshot de parámetros/campos) de `processNode` a `prepareNodeScope`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Construcción del scope y sombreado. |
| Revisión de referencia | `development` en `f96dde76`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `prepareNodeScope`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `prepareNodeScope` construye `currentNamespace`/`currentScope` (la mitad de discovery).
- `processNode` delega esa parte; queda más cerca del collector de demandas.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
