# Protección del comando de proyecto ante LLD

El comando de proyecto `build` todavía acepta únicamente NSLink como opción y
lo registra en su resultado. Mientras no se integre la selección de proyecto,
rechaza una configuración `NSC_LINKER` que apunte a `lld-link`; así no reporta
NSLink mientras el enlace ejecuta otra herramienta. Este rechazo es una
limitación pendiente, no una solución completa del soporte LLD.

La prueba separada comprueba selección explícita de LLD y NSLink, rechazo de
un ejecutable ajeno y el diagnóstico de `build`. Pendiente de construcción y
ejecución agrupada.
