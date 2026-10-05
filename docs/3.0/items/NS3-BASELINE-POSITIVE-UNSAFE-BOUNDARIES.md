# NS3 baseline: límites `unsafe` en fixtures positivas

## Problema

Cuatro fixtures positivas anteriores al endurecimiento de Edition 3 ejecutaban desreferencias,
casts de procedencia o llamadas FFI sin un límite `unsafe` explícito. El compilador las rechazaba
correctamente con NS3030, pero las pruebas seguían describiéndolas como programas válidos.

## Cambio

Se añadieron bloques `unsafe("motivo")` mínimos alrededor de las operaciones peligrosas en T39,
T42, T44 y aliases opacos. Las comprobaciones seguras permanecen fuera de esos bloques y no se
relajó ninguna regla semántica del compilador. La fixture negativa `WritePointee` también atraviesa
deliberadamente el límite `unsafe` para aislar su contrato original: un pointee `const` sigue sin
ser asignable. Por el mismo motivo, `BadReinterpretConst`, `CStyleDiscard` y `OnePastDeref`
atraviesan el límite de auditoría antes de comprobar sus errores estructurales específicos.

## Archivos

- `tests/integration/ns3_t39_frontend_abi/Main.ns`
- `tests/integration/ns3_t42_pointers_linkage/Main.ns`
- `tests/integration/ns3_t42_pointers_linkage/negative/WritePointee/Main.ns`
- `tests/integration/ns3_t42_pointers_linkage/negative/BadReinterpretConst/Main.ns`
- `tests/integration/ns3_t42_pointers_linkage/negative/CStyleDiscard/Main.ns`
- `tests/integration/ns3_t42_pointers_linkage/negative/OnePastDeref/Main.ns`
- `cmake/VerifyNativeSharpT42PointersLinkage.cmake`
- `tests/integration/ns3_t44_metadata_closure/Package.ns`
- `tests/unit/NativeSharp3T04TypeAliasTests.cpp`

## Mejora

Las fixtures positivas documentan el modelo real de seguridad de NativeSharp 3.0 y conservan una
superficie `unsafe` pequeña, auditable y justificada. La expectativa de retorno conserva además el
diagnóstico vigente de contrato completo de puntero, en vez del texto antiguo limitado a `const`.
