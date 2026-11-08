# T08: probar volatile mediante parámetro C calificado

La sintaxis `int volatile*` del fixture se rechazó al parsear; no acredita una
declaración de puntero a volatile en NativeSharp. El fixture usa ahora una
función C que recibe `volatile int*`, escribe y lee por ese puntero. El control
verifica el paso de argumento y mantiene el rechazo de pérdida de calificador.
