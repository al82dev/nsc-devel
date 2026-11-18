> **Vigencia:** NativeSharp tiene un único lenguaje acumulativo. Las etiquetas históricas de edición no seleccionan sintaxis ni semántica; la versión del producto es 1.0.

# NS3 baseline: selección de NSLink

## Problema

Nueve pruebas de NativeSharp reemplazaban `NSC_LINKER` con la ruta histórica
`A:\nslink-devel\build-debug\bin\nslink.exe`. Esto anulaba la ruta elegida por NSCBuilder y hacía
fallar las ejecuciones O0 cuando NSLink se encontraba en `bin\release`, `bin\debug` o en otra ruta
configurada.

## Cambio

Se retiraron las sobrescrituras locales de las pruebas T13 y T14. Cada proceso conserva ahora el
`NSC_LINKER` recibido del entorno de validación, que es el contrato utilizado por el compilador.

## Archivos

- `tests/unit/NativeSharp3T13BitFieldIntegerTests.cpp`
- `tests/unit/NativeSharp3T13CallingConventionTests.cpp`
- `tests/unit/NativeSharp3T13CrossModuleLifetimeTests.cpp`
- `tests/unit/NativeSharp3T13FfiContractTests.cpp`
- `tests/unit/NativeSharp3T13PointerQualifierTests.cpp`
- `tests/unit/NativeSharp3T13PortableSimdTests.cpp`
- `tests/unit/NativeSharp3T13RepresentationTests.cpp`
- `tests/unit/NativeSharp3T13UnsafeAuditingTests.cpp`
- `tests/unit/NativeSharpT14FloatingPointSemanticsTests.cpp`

## Mejora

La prueba ejercita exactamente el NSLink seleccionado por el usuario o por NSCBuilder y deja de
depender de una estructura interna concreta del repositorio de NSLink.
