# Tanda NSC305-G06b — Guard de profundidad de los clonadores

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G06 (paso parcial de robustez; la conversión iterativa completa se difiere).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Evitar que `cloneType`/`cloneNode` desborden la pila con tipos/expresiones patológicamente profundos, convirtiendo el desbordamiento en un error de materialización limpio. |
| Naturaleza | Guard RAII de profundidad compartida (no elimina la recursión; la limita). |
| Comportamiento idéntico | Hasta 1024 de profundidad, idéntico. Más allá, error causal + rollback (mismo patrón que pack-error). |
| Revisión de referencia | `development` en `93957aec`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `cloneDepth_`, `CloneDepthGuard`, `kMaximumCloneDepth`; `cloneType`, `cloneNode`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Miembro `cloneDepth_` compartido por la recursión mutua `cloneType`/`cloneNode`.
- `CloneDepthGuard` RAII incrementa/decrementa; a profundidad > 1024 se registra
  `packMaterializationError_` ("generic materialization exceeds the maximum nesting depth") y se
  devuelve un clon de fallback (tipo `int` o `nullptr`), que `ensure*Specialization` publica y revierte.

## 3. Evidencia y alcance honesto

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
- **ALCANCE**: protege el camino de CLONACIÓN (cuerpos de template con expresiones profundas). Un tipo
  `T<T<…>>` de 5000 niveles desborda ANTES en el parser/traversal (proceso `processTypeSyntax`/parser),
  que es materia de G07/G08 (retirar `processNode`) y del parser (R01–R03), no de G06.

## 4. Pendiente

- Conversión iterativa completa de `cloneType`/`cloneNode` (máquina de frames postorden) — diferida por
  riesgo/valor (robustez para entradas patológicas; sin ganancia medida en casos reales).
