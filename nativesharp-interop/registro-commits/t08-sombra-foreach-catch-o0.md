# Sombras de global C en foreach y catch

El fixture existente importa la global C `shadow_value` y usa el mismo nombre
para variables de `foreach` y `catch` en plantillas. Su consumidor precompilado
comprueba las rutas de foreach y de catch con filtro. `validar-o0-sombras.cmd`
reutiliza ese fixture sin modificarlo, construye productor y consumidor en A:
y ejecutó correctamente los controles en O0. No sustituye la validación completa
del bloque ni acredita otros casos de captura o ámbitos.
