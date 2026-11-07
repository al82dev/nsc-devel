# T08: control C de promoción en O0–O3

El control C de promoción atómica se compilaba sin opción de optimización
explícita. Ahora se compila y ejecuta por separado con Clang O0, O1, O2 y O3;
cada nivel debe producir los resultados esperados antes de validar NativeSharp.
Esto evita atribuir a todos los niveles el resultado observado solo en O0.

`atomic/validar.cmd` pasó con los cuatro controles C y los controles NativeSharp
preparados. Registro `A:/nsc-interop-build/tanda08-atomic-clang-optimizations.log`.
No se reconstruyó NSC y no se validó la totalidad de T08.
