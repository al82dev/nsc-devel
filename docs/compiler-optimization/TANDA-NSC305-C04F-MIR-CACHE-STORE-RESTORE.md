# Tanda NSC305-C04f — store/restore del `MirCacheRecord`

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C04 (caché MIR: persistir/restaurar el registro por callable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `storeMirCacheRecord`/`restoreMirCacheRecord` (registro por implementation fingerprint). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume desde el MIR model). |
| Comportamiento idéntico | Sin cambio (métodos nuevos). |
| Revisión de referencia | `development` en `cb3a7e3e1`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `storeMirCacheRecord`, `restoreMirCacheRecord`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `storeMirCacheRecord` persiste el registro (clave observable + huellas) bajo la huella de
  implementación (la clave estable de partición C5). `restoreMirCacheRecord` lo recupera. Es la capa
  de persistencia que el MIR model consumirá; la serialización completa del cuerpo MIR es la siguiente.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
