# T08: validación dirigida de global C `volatile _Bool`

La construcción incremental de `nsc` y `nsc_namespace_function_llvm_tests`
terminó con código 0; registro `A:/nsc-interop-build/t08-bool-scalar-build.log`.
El grupo record-c terminó con código 0 en O0–O3 y comprobó lecturas y
escrituras cruzadas entre NativeSharp y C; registro
`A:/nsc-interop-build/t08-bool-scalar-record.log`.

El IR O0 declara `@volatile_bool_scalar` como `external global i8` y contiene
lecturas `atomic volatile i8 ... acquire, align 1` y escrituras `atomic
volatile i8 ... release, align 1`. Los controles TLS escalares y TLS atómico
pasaron; registros `t08-bool-scalar-tls.log` y `t08-bool-scalar-atomic.log` en
`A:/nsc-interop-build`. La prueba existente de identidad LLVM pasó 12 checks.
El nuevo indicador también se incluye en la identidad de caché MIR; esta
ejecución no prueba aisladamente un cambio de ese indicador.

La evidencia cubre el global C escalar volatile en modo MS y las configuraciones
probadas. Otras formas, T08-004/015 y la validación integral posterior siguen
pendientes. No se modificó NSLink.
