# Diagnóstico para dirección de slot con apuntado const

La primera validación rechazó `&c_slots[0]`, pero atribuyó el rechazo a un
campo de bits. Se prepara un diagnóstico específico en la indexación cuando
la dirección perdería el calificador anidado. También se hacen estrictos los
patrones de los tres controles negativos: los anteriores podían coincidir con
`const` en el nombre del directorio. Falta repetir la validación agrupada.
