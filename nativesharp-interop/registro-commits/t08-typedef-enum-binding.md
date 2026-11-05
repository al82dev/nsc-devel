# Tanda 08: enlace inicial de typedef a enum

El resolvedor contrasta nombre, proyecto y base entera con el enum importado
antes de expandir un alias. Si etiqueta y typedef nombran el mismo enum, deja
que el binding use la declaración ya indexada. Punteros y calificadores que no
se pueden preservar reciben diagnóstico. Falta validación conjunta y cobertura
de enums C++ y declaraciones incompletas.
