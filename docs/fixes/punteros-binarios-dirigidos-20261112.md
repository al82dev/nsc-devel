# Controles dirigidos de punteros con SDK binario integrado

Sobre la revisión `6beb5c690` se ejecutaron dos pruebas con el compilador
incremental y Core O0 aislados en `A:/nsc-fixes-evidencia`. No se cambió el
compilador para estas comprobaciones.

`shared_receiver_binary_instance.py` pasó: comprueba rechazos conservadores de
llamadas sobre receptores cuyos cuerpos binarios no se pueden verificar y
controles fuente, para constructor y destructor. La prueba no ejecuta los
consumidores; su registro conserva comandos, diagnósticos y hashes antes y
después en `A:/nsc-fixes-evidencia/punteros-receptor-binario-integrado-20261112.log`.
La aceptación o rechazo se limita a las formas concretas comprobadas.

`shared_binary_throwing_direct.py` pasó la matriz de productores y consumidores
O0/O2 para los casos de factoría y limpieza ante excepción incluidos en sus
fixtures: las ejecuciones esperadas devolvieron 42. Evidencia por caso en
`A:/nsc-fixes-evidencia/punteros-throw-binario-integrado-20261112/` y resumen
en el archivo del mismo nombre con extensión `.log`.

Estos resultados no demuestran seguridad completa de las cuatro familias,
concurrencia, rutas de vistas por alias ni vida de módulos descargables. No se
ejecutó una batería completa ni se tocó interoperabilidad C/C++.
