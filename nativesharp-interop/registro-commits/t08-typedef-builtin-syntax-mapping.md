# Tanda 08: mapeo sintáctico inicial de typedef escalar

`cppAliasBuiltinType` determina el token y nombre NativeSharp para escalares C y
punteros con ancho de 64 bits, preservando `const` apuntado cuando hay un solo
nivel. Rechaza por ahora `volatile`, `const` que no puede conservar, tipos no
escalares y anchos no representados; no los transforma silenciosamente. Es una
pieza parcial: faltan las demás representaciones y conectarla a la resolución de
alias. No se ha ejecutado la validación de la tanda.
