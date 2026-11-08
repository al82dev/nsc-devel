# Declaración C incompatible no demandada directamente

Con el encabezado que declara una firma incompatible visible al consumidor,
pero sin llamar a esa declaración, la muestra construye y ejecuta correctamente
la llamada de la plantilla precompilada en O0. Si el consumidor demanda también
su declaración incompatible, el mismo fixture exige el diagnóstico temprano.
Esto registra el comportamiento observado de selección por demanda; no demuestra
una política universal para todas las declaraciones no utilizadas.
