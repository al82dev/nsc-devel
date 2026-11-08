# T08: campos C de record volatiles

La proyeccion excluia un campo C de tipo record cuando estaba declarado `volatile`. Esto ocultaba el miembro en consumo directo y hacia fallar la reconstruccion del catalogo en consumo precompilado. Ahora se preserva el calificador del campo despues de comprobar la identidad y el layout del record importado.

El control `ProbeRecord` comprueba lectura y escritura de miembros escalares anidados. Se corrigio su comparacion booleana y se agrego una comprobacion del IR: los accesos de NSC son `volatile` y el control de Clang lee los mismos tipos de forma `volatile`. El validador ahora exige que terminen correctamente los probes y el productor precompilado, en lugar de tratar sus fallos solo como diagnosticos.

Compilacion incremental de NSC y `pruebas-tanda-08-volatile-campos-c/validar.cmd`: codigo 0. El control principal cubre O0-O3 y tres rutas; `ProbeRecord` y `ProbeOddConsumer` se ejecutaron en O0. Faltan pruebas de miembros anidados en el consumidor solo metadata y en optimizaciones mayores; la tanda 8 permanece abierta.
