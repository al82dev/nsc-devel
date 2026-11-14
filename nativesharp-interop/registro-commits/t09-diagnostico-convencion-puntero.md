# T09-003: diagnóstico de punteros C/C++ con convención no admitida

`QualifiedTypeNames.cpp` informaba solamente `unsupported C callback calling
convention` al proyectar un alias de puntero a función cuya convención no tiene
equivalente NativeSharp (`swiftcall`, `swiftasynccall`, `intel_ocl_bicc`). Ahora el
mensaje nombra la convención observada por Clang y el alias calificado, por ejemplo
`unsupported C callback calling convention 'swiftcall' for alias 'T09Cc::T09CcPtr'`.
El rechazo, el identificador de diagnóstico y el texto previo (que se conserva como
prefijo) no cambian.

Validación: la matriz `pruebas-tanda-09-convenciones/validar-matriz.py` muestra el
mensaje nuevo en las seis filas de puntero rechazadas y ninguna aceptación con
convención distinta (`A:/temp/nsc-t09-convenciones-73ucdhru`). Construcción
incremental `3.0.0+build.20261114T160753Z.d3d9c0dc5fdc10ba`; resultados de la
validación agrupada en [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).
