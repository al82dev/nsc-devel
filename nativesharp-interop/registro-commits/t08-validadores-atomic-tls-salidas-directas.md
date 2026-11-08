# T08: fallos de ejecución en bucles de validadores atómicos

Una copia aislada devolvió cero cuando un consumidor compilado terminó con 42.
Las comprobaciones directas de error de los seis validadores atómicos y TLS
utilizan el destino explícito de fallo. Los casos precompilados conservan la
restauración cuando corresponde. Pendientes pruebas inducidas y regresiones.
