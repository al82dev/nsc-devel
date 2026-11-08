# T08: conversión flexible con tipos y metadata

En `58654ec4f`, `pruebas-tanda-08-flexible-c/validar-decay.cmd` terminó con
código 0. Los casos directos `int`, `double`, record, enum y `_Bool` escriben
por el puntero y C observa el valor O0–O3. El `int` pasó con productor
precompilado y consumidor con header compartido o solo metadata, ocultando
temporalmente la copia del header. La dirección subalineada se rechazó.

Los tipos adicionales no se probaron en los consumidores precompilados. La
conversión de elementos const/volatile, expresiones y condiciones adicionales
continúa pendiente; este resultado no cierra la tanda.
