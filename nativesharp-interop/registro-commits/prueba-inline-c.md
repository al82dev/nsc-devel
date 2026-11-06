# Funciones C static inline

El fixture prueba una función simple, otra que combina una función inline y un símbolo externo, y una con estado local estático persistente. Se ejecuta en O0–O3; preparar el fixture no demuestra soporte.

El primer intento falló en O0: NSC no reconoce los tres identificadores `static inline`. Se añadió una operación aislada de sesión que pide a CodeGen de Clang generar varias funciones en un mismo módulo IR; `nsc.cpp-interop.session` pasó con tres funciones seleccionadas, una llamada entre ellas y un contador estático. La operación todavía no proyecta símbolos para NSC ni incorpora el módulo al enlace: el fixture de extremo a extremo continúa fallando y el ítem sigue abierto.

Se amplía el encabezado con una variable y una función internas no utilizadas que referencian un símbolo externo sin definición. El enlace del consumidor debe seguir funcionando: una declaración que no se usa no debe imponer esa dependencia. Pendiente de validación agrupada.

Validación posterior, build `3aa5707a6d2b2cc7`: `validar.cmd` pasó en O0–O3 para el ejecutable directo y para la biblioteca precompilada con su consumidor, aun sin definición de `external_unused`. La prueba acredita estos casos y no cierra el ítem ni la tanda.

Se añade un segundo productor precompilado que incluye el mismo encabezado; el consumidor alterna llamadas y comprueba que ambos contadores y variables internas conservan estado independiente. Pendiente de la validación agrupada.
