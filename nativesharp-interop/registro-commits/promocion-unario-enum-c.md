# Promoción unaria de enum C en HIR iterativo

El caso de 8 bits devolvió 5: el IR negaba i8 y almacenaba el resultado antes
de promocionar. La ruta iterativa no usaba el cambio de la ruta recursiva. Se
prepara la conversión previa en ambas rutas; pendiente construir y confirmar
que el IR amplíe el enum antes de negar y que los tests terminen O0–O3.
