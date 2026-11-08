# T08 H3: versión de metadata para enumeradores C anónimos

NSMETA 1.47 identifica el catálogo de dependencias que incorpora enumeradores C anónimos. Los lectores versionados rechazan esas entradas bajo 1.46 y conservan los resultados de salida ante ese rechazo. El verificador limita el tipo de cada entrada a enteros C representables y exige identidad.

La construcción incremental y `nsc_cpp_c_identity_tests` pasaron. El control dirigido confirma rechazo en 1.46 y lectura en 1.47; las rutas anteriores sin enumeradores conservan la codificación previa. La batería completa de metadata y de la tanda está pendiente.
