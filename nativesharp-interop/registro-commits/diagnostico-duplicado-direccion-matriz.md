# Diagnóstico duplicado al tomar dirección de matriz C

En las pruebas de dirección de fila y slot se emitían tanto el error específico
de const como el mensaje genérico de bitfield. La indexación marcaba el
resultado válido antes de informar el error; se invalida ese resultado para que
el operador de dirección no produzca el mensaje adicional. El fixture exige
el diagnóstico específico y la ausencia del genérico. Falta validar.

Con build `6b5448c0a735018f`, la batería de arreglos const confirmó ambos
diagnósticos específicos sin el mensaje genérico de bitfield; la regresión de
layout pasó O0–O3. Esto no acredita otros operandos de dirección.
