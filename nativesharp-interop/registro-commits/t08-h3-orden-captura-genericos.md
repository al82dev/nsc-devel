# T08 H3: orden de captura de dependencias C

Se registra una consecuencia concreta del orden de fases: el colector de tipos genericos indexa declaraciones AST antes del name lookup y omite referencias del mismo modulo en su lista de tipos externos. El observador de funciones y valores se construye por separado. No se ha demostrado que sus demandas cubran las plantillas no instanciadas ni que resuelvan sombras locales.

Es evidencia por lectura para continuar la implementacion; no cambia codigo ni formato, no sustituye las pruebas y no cierra H3 ni la tanda 8.
