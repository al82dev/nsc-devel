# T08: acceso concurrente a TLS C atómico `short`

Un hilo C recibe la dirección del TLS `_Atomic(short)` del hilo propietario y
lo incrementa 10000 veces. En paralelo, NativeSharp lo incrementa otras 10000
veces y espera al hilo C antes de comprobar el valor 20000. El hilo propietario
permanece vivo hasta completar la prueba; el total cabe en `short`.

`atomic/validar.cmd` pasó para los ejecutables NativeSharp O0–O3 y los controles
previos sin reconstruir NSC. Registro
`A:/nsc-interop-build/tanda08-atomic-narrow-concurrency.log`. La prueba no fuerza
una intercalación concreta ni sustituye el gate completo de T08.
