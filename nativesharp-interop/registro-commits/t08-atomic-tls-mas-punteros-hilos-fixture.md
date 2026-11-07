# T08: aislamiento por hilos de punteros atómicos TLS adicionales

Se prepara un segundo hilo C que comprueba sus punteros TLS iniciales, los
asigna a datos distintos y verifica sus valores. El hilo NativeSharp espera al
trabajador y verifica que sus propios punteros y valores no cambiaron. El
control se integra en la matriz O0–O3 existente, pero aún no se ha ejecutado.
No acredita aislamiento ni modifica producción o NSLink por sí solo.
