# T08 H3: límite de ámbito de for

El caso positivo añade un `for` con una variable del mismo nombre que la
función C y mantiene una llamada a esta después del bucle. El nuevo resultado
esperado es 182. El control permite detectar si la sombra se extiende por error
fuera del ámbito del `for`; la ejecución se registra separadamente.
