# Tanda NSC305-C01a — `sourceFingerprint` (clave C1)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (caché de templates parseados: clave por huella del texto fuente).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `sourceFingerprint` (SHA-256 del texto fuente del template, reconstruido desde los tokens del lexer) al record de template, base de la clave C1. |
| Naturaleza | Añadido (sin cambio de conducta; la huella se calcula y se guarda). |
| Comportamiento idéntico | Recogida de templates (solo se calcula una huella extra). |
| Revisión de referencia | `development` en `184e4d26`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericTemplateRecord::sourceFingerprint`, `computeTemplateSourceFingerprint`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `computeTemplateSourceFingerprint` concatena los lexemas de los tokens en el rango del nodo y
  calcula su SHA-256 (`basic::sha256`).
- `collectTemplates` lo guarda en `sourceFingerprint` para tipos y métodos genéricos.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
