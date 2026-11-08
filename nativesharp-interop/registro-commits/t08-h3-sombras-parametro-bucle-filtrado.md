# T08 H3: referencias a parámetros y variables de for

Se amplía la comprobación de ámbitos de plantillas para reconocer parámetros
de métodos y funciones dentro de su cuerpo, así como declaraciones locales
del inicializador de `for` en la condición, el incremento y el cuerpo del bucle.
No se extiende el ámbito al inicializador mismo ni al código posterior al bucle.

Las referencias no calificadas que se demuestran locales no se entregan como
demandas de importación C. Las demás formas siguen pendientes de verificación.
La construcción y los controles agrupados se registrarán por separado.
