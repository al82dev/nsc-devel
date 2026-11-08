# T08 H3: identidad física del origen del enumerador anónimo

La conciliación basada en huella y USR aceptaba dos archivos distintos con contenido idéntico. El productor conserva también la ruta de origen. El consumidor exige que la ruta viva y la registrada identifiquen el mismo archivo mediante `filesystem::equivalent`, además de comprobar la huella, USR, nombre, tipo y valor.

La matriz de identidad terminó con código 0: el mismo archivo se concilia; dos archivos distintos con contenido diferente o idéntico conservan la ambigüedad. Si el archivo del productor ya no está disponible, esta evidencia no permite conciliarlo; el consumo sin encabezado permanece disponible. No se ha cerrado la tanda 8.
