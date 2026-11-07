# T08-002: evidencia nueva para la regla de typedef (H1)

El modo `--typedef-matrix` comprueba además la forma del USR de typedef con
nombres de encabezado que contienen `@` o un espacio, y con un typedef generado
por una macro de un encabezado común.

Clang copia el nombre del archivo de expansión tal cual. La regla propuesta
acepta el espacio, rechaza el `@` (falso negativo conservador) y no se ve
afectada por las macros. La evidencia se añade a `T08-002-PROPUESTA-TYPEDEF.md`.
La regla sigue sin aplicarse.
