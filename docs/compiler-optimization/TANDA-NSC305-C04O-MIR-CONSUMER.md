# Tanda NSC305-C04o — Consumidor del caché MIR

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: store/restore por callable en ProjectOopMirModel).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `setMirCacheRoot`/`storeMirFunctions`/`restoreMirFunctions` y persistir en el build. |
| Naturaleza | Añadido (sin cambio de conducta con caché deshabilitada). |
| Comportamiento idéntico | Sin caché configurada, igual que antes. |
| Revisión de referencia | `development` en `9f8f391c4`. |
| Archivos | `include/nativesharp/Frontend/ProjectOopMirModel.h`, `src/Frontend/ProjectOopMirModel.cpp`. |
| Símbolos | `setMirCacheRoot`, `storeMirFunctions`, `restoreMirFunctions`, `mirCache_`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ProjectOopMirModel` posee su `TemplateParseCache`; `storeMirFunctions` persiste las funciones por
  huella de implementación (clave estable C5) al final del `build`; `restoreMirFunctions` valida el
  round-trip y cuenta hits. Cierra la tanda C04.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
