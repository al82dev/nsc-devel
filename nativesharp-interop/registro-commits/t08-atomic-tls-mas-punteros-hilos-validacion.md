# T08: aislamiento por hilos de punteros atómicos TLS adicionales

`validar-punteros-tls.cmd` terminó con código 0 y marcadores O0–O3. En cada
nivel, el hilo C observó sus tres punteros iniciales nulos, los asignó a datos
propios y leyó esos valores. Tras esperar al hilo C, NativeSharp comprobó que
sus punteros y valores seguían siendo los originales. La prueba también volvió
a ejecutar el control previo de operaciones e IR de los tres punteros.

No acredita apuntados calificados, DLL o todos los posibles hilos y escapes.
El gate agrupado sigue pendiente y T08-004 permanece abierto. NSLink no se
modificó.
