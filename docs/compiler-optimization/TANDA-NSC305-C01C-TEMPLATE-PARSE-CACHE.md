# Tanda NSC305-C01c — `TemplateParseCache` (I/O de la caché C1)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (caché de templates parseados: I/O de disco con publicación atómica).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `TemplateParseCache` (store/restore de bytes con metadata NSCTPL, SHA-256, publicación atómica y cuarentena). |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (clase nueva). |
| Revisión de referencia | `development` en `101576ce`. |
| Archivos | `include/nativesharp/Frontend/TemplateParseCache.h`, `src/Frontend/TemplateParseCache.cpp`, `CMakeLists.txt`. |
| Símbolos | `TemplateParseCache` (`setRoot`/`enabled`/`restore`/`store`). |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `TemplateParseCache` mueve bytes bajo una clave C1 (64 hex), con metadata `NSCTPL 1` (key +
  sha256 + size), rename atómico y cuarentena ante corrupción. Comparte la convención de
  `CompilationArtifactCache`.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
