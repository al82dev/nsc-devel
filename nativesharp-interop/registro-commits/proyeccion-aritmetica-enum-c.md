# Aritmética sobre enum C importado

La prueba real rechazó sumas sobre enums C signed y unsigned. Se prepara usar
la representación subyacente capturada por Clang al resolver el tipo numérico,
sin aplicar esa regla a enums NativeSharp. El lowering y el resultado frente a
la operación C todavía requieren validación agrupada.
