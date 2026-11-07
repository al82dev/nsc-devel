# T08: validación de bool C precompilado e identidad MIR

La construcción incremental de `nsc_namespace_function_llvm_tests` terminó
con código 0; registro `A:/nsc-interop-build/t08-bool-cache-unit-build.log`.
El ejecutable pasó 14 checks, incluida la diferencia de identidad al cambiar
el indicador de almacenamiento C bool volatile y su coincidencia con el índice.

El fixture `pruebas-tanda-08-record-c/precompiled-bool/validar.cmd` terminó con
código 0: productor precompilado y consumidor sin el header C pasaron O0–O3,
con lecturas y escrituras observadas desde C y NativeSharp. El IR del productor
declaró `@flag` como i8 y mostró lecturas acquire y escrituras release de byte,
incluida la operación compuesta. Registro:
`A:/nsc-interop-build/t08-bool-scalar-precompiled.log`.

El control cubre biblioteca estática y los casos ensayados; no acredita DLL ni
caché de artefactos activa. T08 y su validación integral posterior siguen
pendientes. No se modificó NSLink.
