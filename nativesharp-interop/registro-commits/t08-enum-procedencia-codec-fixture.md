# Control del codec de procedencia de enums

La prueba de identidad usa el enum real `CIdKind` capturado por Clang para
comprobar una vuelta por el codec preparado junto con carriers anónimos y una
dependencia. Incluye rechazo de bytes adicionales y de un hash inválido. Los
controles todavía no se han compilado ni ejecutado; el codec sigue sin conectar
a NSMETA.
