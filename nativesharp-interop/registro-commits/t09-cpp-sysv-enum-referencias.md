# T09: referencias equivalentes de enum class en SysV

## Cambio

Una llamada variádica SysV que usa un `enum class` C++ fallaba cuando dos referencias observaban el mismo encabezado. Se concilian las observaciones de enum por USR, nombre, base, representación y enumeradores, y las dependencias equivalentes de función o alias mediante identidad, firma y plan ABI. La conciliación adicional está acotada al caso con enum scoped; se conservó el rechazo de sesiones C duplicadas exigido por la prueba existente. El catálogo de NSMeta conserva su contrato vigente.

## Validación

- Compilación incremental exitosa: `A:/temp/t09-cpp-record-replay-build.log`.
- `validar-referencias.py`: dos encabezados equivalentes con llamada directa y callback, comparación con Clang en estática y DLL O0–O3, y 16 ejecuciones precompiladas por ruta con y sin encabezado. El mismo script comprueba que un enumerador incompatible produce error: `A:/temp/nsc-t09-sysv-enum-referencias-_tzgs6u5`.
- Una primera versión aceptaba duplicados C y falló la prueba interna ABI con código 186; se acotó la conciliación y se repitieron las tres pruebas internas con código 0: `A:/temp/t09-scoped-enum-unit-tests.log`.
- Regresiones precompiladas de record C y C++: `A:/temp/nsc-t09-sysv-indirecta-record-pre-4viholsl` y `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-celu6hhr`.

## Límites

La prueba usa dos referencias al mismo encabezado, una de ellas sin biblioteca, y no acredita todas las combinaciones de bibliotecas y encabezados. Cuando ambas referencias del productor declararon la misma biblioteca, el enlace informó símbolos duplicados: `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-precompilado-x55wv9kw`. Esa limitación permanece abierta; no se modificó NSLink. La prueba SysV comprueba registros, no `va_arg`. T09-003, T10-002 y T10-003 siguen abiertos.
