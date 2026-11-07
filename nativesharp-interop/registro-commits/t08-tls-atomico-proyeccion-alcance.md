# Alcance: primera proyección TLS atómica

Se conecta el caso C TLS `_Atomic(int)` con tamaño y alineación comprobados al
transporte e IR preparados, con barrera para direcciones y argumentos por
referencia. Las demás formas y operaciones siguen pendientes; el fixture debe
compararse con Clang antes de acreditar este caso.
