# Tanda 08: identidad de record anidado en typedef

La comparación usa ahora la identidad canónica con `+` entre records anidados,
como `TypeIdentity`, y conserva la escritura con puntos para el código fuente.
Ambas formas tienen responsabilidades distintas; usar puntos para comparar la
clave canónica impedía reconocer un record anidado. Validación agrupada pendiente.
