# Tanda NSC305 — Índice de especializaciones por nombre generado

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Micro-optimización de complejidad en la consulta de especializaciones de tipo (fases posteriores).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Sustituir el barrido lineal `find_if` de `specializationForTypeKey` (que busca por `specializedName`) por un índice O(1) construido al final de `build()`. |
| Naturaleza | Refactor de complejidad no funcional (O(S) → O(1) por consulta). |
| Comportamiento idéntico | Resultado de `specializationForTypeKey` (primer `specializedName` gana). |
| Revisión de referencia | `development` en `47099218`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `specializationForTypeKey`, nuevo `specializationBySpecializedName_`, fin de `build()`. |
| Pruebas | `Host.Frontend` (544) + caso de proceso `TL-LANG-287` con `TMP` en `A:`. |

## 2. Cambio

- `specializationBySpecializedName_` (nombre generado → ordinal) se construye una vez al final de
  `build()`, cuando toda materialización/rollback ya ocurrió; `emplace` conserva el primer nombre.
- `specializationForTypeKey` consulta el índice en lugar de `find_if(specializations_)`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**; `TL-LANG-287` (proceso) PASS con
  `TMP`/`TEMP` en `A:`.
- **HECHO COMPROBADO**: los 607 fallos de la corrida completa sin `TMP` en `A:` son ambientales
  ("could not create isolated project directory"); con `TMP` en `A:` el caso comprobado pasa.

## 4. Siguiente acción

NSC305-G06: clonadores `cloneNode`/`cloneType` iterativos (plan en el handover, esfuerzo multi-ronda).
