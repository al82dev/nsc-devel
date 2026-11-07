# Protección del comando de proyecto ante LLD

El comando de proyecto `build` todavía acepta únicamente NSLink como opción y
lo registra en su resultado. Mientras no se integre la selección de proyecto,
rechaza una configuración `NSC_LINKER` que apunte a `lld-link`; así no reporta
NSLink mientras el enlace ejecuta otra herramienta. Este rechazo es una
limitación pendiente, no una solución completa del soporte LLD.

La prueba separada pasó con build `0934f50429ac4352`: comprobó selección
explícita de LLD y NSLink, rechazo de un ejecutable ajeno y el diagnóstico de
`build`. La suite de sesión Clang también pasó 1/1. El soporte real de LLD en
`build` sigue pendiente.

Posteriormente se implementó la selección explícita de LLD para `build`; la
matriz de proyecto y los rechazos de selecciones contradictorias pasaron con
build `2c5d0967206b6e62`. La selección automática sigue pendiente.
