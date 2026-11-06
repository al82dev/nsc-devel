# Funciones C static inline

El fixture prueba una función simple, otra que combina una función inline y un símbolo externo, y una con estado local estático persistente. Se ejecuta en O0–O3; preparar el fixture no demuestra soporte.

El primer intento falló en O0: NSC no reconoce los tres identificadores `static inline`. Se añadió una operación aislada de sesión que pide a CodeGen de Clang generar varias funciones en un mismo módulo IR; `nsc.cpp-interop.session` pasó con tres funciones seleccionadas, una llamada entre ellas y un contador estático. La operación todavía no proyecta símbolos para NSC ni incorpora el módulo al enlace: el fixture de extremo a extremo continúa fallando y el ítem sigue abierto.
