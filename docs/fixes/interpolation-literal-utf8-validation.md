# Validación de bytes en texto interpolado

LKF-582: la interpolación ordinaria aceptaba bytes de control y secuencias UTF-8 inválidas
en texto literal que una cadena ordinaria rechazaba. La interpolación raw rechazaba controles,
pero tampoco validaba secuencias UTF-8 en texto literal.

Ambas formas validan ahora los bytes de sus segmentos literales. La forma raw conserva sus
reglas anteriores para tabulación y saltos de línea. Los bytes dentro de los huecos se
conservan para que el lexer de la expresión los procese posteriormente; las secuencias de
escape de la forma ordinaria conservan su comportamiento anterior.

La prueba incremental `nsc_lexer_tests` pasó 57 comprobaciones, incluidos UTF-8 inválido,
control directo, texto válido, escapes y preservación byte a byte de huecos. La CLI de la
copia rechazó las reproducciones previas con controles y UTF-8 inválido. Se actualizó el
control caracterizador `TL-LEX-091` y sus descripciones de inventario y manifiesto; el
runner de componentes de esa suite no se ejecutó en esta pasada dirigida.
