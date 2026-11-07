# T08-015: controles de división y desplazamientos de `_Bool` C

El fixture añade `/=`, `%=`, `<<=` y `>>=` con operandos `int` positivos y
comprueba resultados de cero y no cero. El verificador IR exige los cuatro
operadores i32, dieciséis lecturas acquire y escrituras release de byte y
once promociones y normalizaciones i32. Conserva el control de bool nativo.

Está preparado sin construcción ni ejecución. No cubre valores para los que C
no define el resultado y no cierra el ítem ni la tanda.
