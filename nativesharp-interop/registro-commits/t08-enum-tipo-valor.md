# Comparación de tipo de valores enum

Las definiciones enum con el mismo valor numérico no se consideran equivalentes
si Clang reporta distinto tipo, representación o alineación para un enumerador.
El caso se mantiene como conflicto diferido; queda pendiente su validación.
