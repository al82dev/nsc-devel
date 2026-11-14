# Token inválido en literales UTF-8 tipados

LKF-424: el lexer de `u8` convertía incondicionalmente el resultado de analizar la cadena
en un token `Utf8StringLiteral`, incluso cuando el análisis ya había emitido un error y
devuelto `Invalid`. Ahora conserva el token inválido original, incluidos rango, texto y
diagnósticos. Los literales válidos mantienen su tipo de token anterior.

La prueba dirigida `nsc_lexer_tests` pasó 49 comprobaciones, incluidos UTF-8 inválido en
formas ordinaria y raw, ambas formas sin cierre, escape desconocido y un control válido.
El compilador de la copia confirmó mediante `--dump-tokens` que las formas inválidas
emiten `Invalid` y terminan con código 1, y las válidas emiten `Utf8StringLiteral` y
terminan con código 0. El ejecutable usado en el layout temporal coincide por SHA-256
con el del build aislado.
