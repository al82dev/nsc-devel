# T08: aislar el conflicto de alias en el control de nombres

El encabezado negativo de `NAlias` declaraba además un `NMood` independiente.
La nueva comprobación de procedencia rechazaba primero ese enum, por lo que la
prueba ya no alcanzaba el conflicto de alias que pretende comprobar.

Se elimina del encabezado negativo la declaración ajena al caso; el enum sigue
disponible desde la biblioteca precompilada. La expectativa de conflicto de
alias permanece intacta. El control dirigido está pendiente de ejecución.
