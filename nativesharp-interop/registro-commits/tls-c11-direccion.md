# Dirección de variable TLS C11

La variante generada del fixture de TLS C11 también solicita la dirección
del global desde NativeSharp y mediante una función C. La prueba exige que
ambas direcciones coincidan para el hilo actual y que el puntero observe la
escritura, además de conservar el control C y la comprobación de aislamiento.
La reproducción inicial aislada pasó O0–O3 con NSC build
`b151b46f0be04227` y LLD desde A:. No se modificó la fuente del fixture
original ni NSLink. La ejecución mediante el generador persistido está
pendiente; este caso no implica cierre de TLS ni de la tanda 8.
