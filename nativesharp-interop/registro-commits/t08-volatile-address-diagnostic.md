# Diagnóstico para dirección de almacenamiento volatile

El rechazo de la dirección de un campo volatile informaba incorrectamente que
el operando era un bit field o temporal. El análisis ahora informa que faltan
punteros calificados cuando conoce que el almacenamiento es volatile. La prueba
debe comprobar el mensaje y repetirse junto con las demás de la tanda.
