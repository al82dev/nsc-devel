# T08 H3: enumeradores C anónimos en plantilla precompilada

El productor selecciona las constantes C anónimas visibles demandadas por plantillas públicas, conserva módulo, referencia, USR, tipo y valor y publica el catálogo bajo NSMETA 1.47. El consumidor reconstruye el valor para la búsqueda de nombres de la plantilla. El constructor habilita la versión requerida al solicitar el catálogo.

El validador dirigido de identidad pasó: `ConsumerAnonymousConstant` y `CConstAnonTemplate` compilaron y se ejecutaron. Un inspector confirmó `GenLoose` en el NSMETA real. La regresión agrupada de dependencias de plantilla pasó en O0–O3. No se acredita toda la cobertura de identidad ni se cierra la tanda 8.
