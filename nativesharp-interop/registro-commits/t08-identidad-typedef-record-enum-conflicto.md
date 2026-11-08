# Identidad de typedefs C incompatibles

El compilador de desarrollo en `A:\nsc-interop-build\tanda1-ON` se comprobó
con `--check-project-method-bodies` y Clang de `A:\al82dev-clang`.

- La muestra existente `T06StructDiffersXY` rechaza definiciones incompatibles
  de un mismo record (`referenced C/C++ record has conflicting or unverified definitions`).
- `T09OtherTagXY` rechaza dos alias con el mismo nombre para records distintos
  (`conflicting C/C++ type alias`).
- `typedefs/EnumConflict/validar.cmd` acepta el control con un encabezado y
  rechaza un alias hacia enums distintos y un enum homónimo con valores distintos.
  El validador terminó con código 0.

La comprobación es de análisis directo; el consumo precompilado sigue pendiente.
