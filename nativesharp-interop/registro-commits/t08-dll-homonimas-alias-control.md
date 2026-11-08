# Control de DLL homónimas con nombres independientes

Se amplió el fixture de DLL homónimas con un control C que copia los dos
binarios `api.dll` a nombres diferentes en el directorio del ejecutable y los
carga mediante rutas absolutas derivadas de ese ejecutable. El control comprueba
que los módulos son distintos y que sus funciones observan los valores TLS 3 y
7. Se ejecutó desde `A:\` para no depender del directorio de trabajo.

`validar-mismo-nombre.cmd` terminó con código 0: pasaron el control anterior,
el nuevo control y la comprobación del diagnóstico actual de NSC. El control
demuestra la viabilidad de nombres de carga independientes para estos dos
binarios; no demuestra por sí solo la integración en NSC ni el comportamiento
de dependencias transitivas o importaciones directas. El rechazo de NSC y la
casilla principal permanecen abiertos.
