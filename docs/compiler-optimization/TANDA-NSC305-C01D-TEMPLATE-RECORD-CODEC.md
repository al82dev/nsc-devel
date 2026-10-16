# Tanda NSC305-C01d — `TemplateRecordCodec` (serialización relocatable del record)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: C01 (serialización compacta sin punteros del record de template).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `serializeTemplateRecord`/`deserializeTemplateRecord` (formato binario compacto, sin punteros) para la metada escalar/string del record. |
| Naturaleza | Añadido (sin cambio de conducta; aún no se consume). |
| Comportamiento idéntico | Sin cambio (codec nuevo). |
| Revisión de referencia | `development` en `ea350362`. |
| Archivos | `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `serializeTemplateRecord`, `deserializeTemplateRecord` (+ helpers de bytes). |
| Pruebas | `Host.Frontend` completo (544 casos, sin regresión). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Codec binario (uint8/uint32/uint64 + strings + vectores de strings) que serializa la identidad del
  record (kind/accessibility/ordinales/nombres/parámetros/claves/huellas). El subárbol AST
  (syntax/extensionSyntaxes/parameterSyntaxes/constraints/externalTypeReferences) queda para la
  siguiente capa del codec.

## 3. Evidencia

- **HECHO COMPROBADO**: compila y `Host.Frontend` **544 PASS / 0 FAIL**.
