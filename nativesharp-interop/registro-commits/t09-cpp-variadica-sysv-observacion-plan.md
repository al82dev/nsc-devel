# T09: observacion de plan C++ variadico SysV

## Cambio parcial

El observador ABI de NSC puede consultar a Clang el plan de una funcion libre C++ `sysv_abi` variadica, incluso si esta en un namespace, y de un argumento record C++ global trivial. La busqueda exige un unico candidato por nombre y tipo canonico; el argumento se concilia por USR, tipo, tamaño y alineacion, y se rechazan records C++ no triviales o no estandar. El plan observado incluye el simbolo C++ y las piezas fisicas. La observacion por si sola no autoriza ni implementa la llamada completa.

## Validacion

- Compilacion incremental de NSC y pruebas ABI: `build 0`, registro `A:/temp/t09-regcall-reconcile-build.log`.
- `nsc.cpp-interop.clang-abi-probe`: aprobado 1/1. Incluye plan con `{double,int}`, simbolo, dos piezas y rechazos de USR incorrecto, destructor no trivial y declaracion ambigua; conserva los controles C existentes.
- Dieciseis consumidores precompilados de la firma C++ escalar existente: `A:/temp/nsc-t09-cpp-variadica-sysv-precompilado-s66856gh`.
- Regresion C SysV variadica, incluidas rutas precompiladas y rechazo de record NativeSharp: `A:/temp/nsc-t09-sysv-varargs-7rirs023`.

## Pendiente

La seleccion semantica, conciliacion de origen del record, llamada y reconstruccion precompilada del record C++ no se han habilitado. La reproduccion `pruebas-tanda-09-cpp-variadica-sysv-record` sigue pendiente y T09-003 y T10-003 permanecen abiertos. No se cubren records no triviales, records C++ en namespaces ni busqueda de funciones homonimas ambiguas.