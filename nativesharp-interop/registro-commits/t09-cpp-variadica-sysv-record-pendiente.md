# T09: record C++ opcional en funcion variadica SysV, pendiente

## Reproduccion

La funcion libre C++ `T09Cpp::sum(int,...)`, con convencion `sysv_abi`, recibe por valor un `T09VarValue { double x; int y; }` C++ trivial global. Un receptor de ensamblador comprueba la parte flotante, la parte entera y el contador SSE; el llamador conserva el valor original. La prueba compara Clang 23.1.0 con NSC para enlace estatico y DLL O0 a O3 y dispone de una ruta precompilada todavia no validada.

- Los ocho controles Clang pasan: `A:/temp/nsc-t09-cpp-variadica-sysv-record-3gdpu1wa`.
- NSC rechaza la llamada por sobrecarga no viable antes de generar codigo: `A:/temp/nsc-t09-cpp-variadica-sysv-record-0qi8lwsl/estatica-O0-ns-ir.log`.
- El fixture `pruebas-tanda-09-cpp-variadica-sysv-record` es una reproduccion pendiente. Su ejecucion completa con NSC se espera fallida hasta corregir el soporte; no es evidencia de implementacion.

## Fronteras observadas y siguiente paso

`ProjectMethodBodyModel::classifyCVariadicPromotion` admite registros importados C, no este record C++. `planCppVariadicCall` y `resolveVariadicArgumentOrigins` filtran sesiones y origenes C; `ClangSession::cVariadicCallAbiPlan` rechaza AST C++. El camino precompilado reconstruye declaraciones C y exige que nombre y simbolo coincidan, condicion que no cubre el nombre C++ mangled. Estas guardas aparecen en etapas diferentes: retirarlas sin conciliar elegibilidad del record, identidad y layout, observacion de Clang y transporte del plan podria emitir un ABI incorrecto.

Siguiente paso: delimitar e implementar el soporte de records C++ triviales en las etapas necesarias, con identidad y layout verificados; conservar rechazos de records no soportados y cubrir ruta viva, precompilada y regresiones C. No se ha implementado ni cerrado T09-003 o T10-003. Esta muestra no usa `va_arg` y no prueba otros layouts o records no triviales.