# Comprobación de alineación de callback volatile

Se prepara una comprobación de campo con identidad de puntero a función,
modo volatile MS y alineación efectiva mínima de ocho bytes. Un campo anidado
o empaquetado utiliza la alineación calculada de su dirección; si no acredita
el mínimo, no se selecciona orden atómico. Falta conectar y validar el IR.
