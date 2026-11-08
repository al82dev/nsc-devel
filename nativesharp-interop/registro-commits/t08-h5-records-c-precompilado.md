# T08-002 H5: records C compatibles mediante biblioteca precompilada

Un productor importa dos encabezados con la misma definición de `struct CTdF`
y publica métodos que reciben y devuelven su puntero. Los consumidores prueban
lectura y escritura del campo por la biblioteca y directamente, con el
encabezado y solo con metadata.

El control `pruebas-tanda-08-identidad-c/records-precompiled/validar.cmd`
pasó en los dos modos y O0–O3, código 0 y `VALIDACION_CALL_OK`. Su salida está
en `A:/nsc-interop-build/tanda08-h5-permanente-20261107/records-precompiled`.
Se incorporó al validador de identidad. No hubo cambios de producción y esta
prueba no cierra T08-002 ni sustituye la batería completa de la tanda 8.
