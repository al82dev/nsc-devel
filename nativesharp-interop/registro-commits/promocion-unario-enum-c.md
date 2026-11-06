# Promoción unaria de enum C en HIR iterativo

El caso de 8 bits devolvió 5: el IR negaba i8 y almacenaba el resultado antes
de promocionar. La ruta iterativa no usaba el cambio de la ruta recursiva. Se
prepara la conversión previa en ambas rutas; pendiente construir y confirmar
que el IR amplíe el enum antes de negar y que los tests terminen O0–O3.

Con `5bcb2fb4f02458e7` el IR amplía `i8` a `i32` antes de la negación. El
fixture pasó O0–O3 con negación, desplazamiento y máscara para enum de 8 bits;
también pasaron complemento y desplazamiento con enum como conteo. Es evidencia
dirigida y no sustituye la validación completa de la tanda.
