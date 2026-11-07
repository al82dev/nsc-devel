# T08-015: controles de `_Bool` C compuesto con `int`

El fixture amplía el caso existente con siete operaciones para comprobar los
seis operadores `+=`, `-=`, `*=`, `&=`, `|=` y `^=`. Los casos con dos y con
resultado negativo distinguen la promoción a i32 de convertir el operando a
bool antes de operar. Se mantienen las comprobaciones de bool con bool y el
rechazo de bool nativo.

El verificador exige once cargas acquire y escrituras release de byte y siete
promociones y normalizaciones i32. Está preparado, sin construir ni ejecutar;
no acredita todavía el cambio ni cubre otros operadores.
