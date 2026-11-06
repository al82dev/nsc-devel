# Planificación y enlace de funciones C inline

El driver reúne las funciones inline seleccionadas por proyecto y sesión, emite un objeto conjunto por sesión e incorpora los objetos al bundle común de ejecutables y bibliotecas. Esto conserva en un módulo el estado interno compartido. La selección se contrasta con el inventario y requiere una sesión viva; la ejecución y los casos precompilados se validan por separado.
