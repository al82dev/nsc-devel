# Incremento de puntero a C incompleto

Con el compilador anterior, `Opaque* value = opaque_get(); value++;` llegó al
descenso y falló por no tener tamaño de elemento. Se preparó la comprobación
en `analyzeIncrementOrDecrement` para structs y unions C importados e
incompletos. El caso queda pendiente de la validación agrupada.
