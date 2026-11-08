# T08-002: control de captura por sombras locales

El diagnóstico construye productor y consumidor en O0–O3. Comprueba el valor
en ejecución, inspecciona el catálogo con el decodificador de NSC y examina la
biblioteca con `llvm-nm` buscando una función C inline sintética. Código 1
señala captura o emisión indebida; código 2 señala fallo de preparación o
compilación. No forma parte del gate mientras el defecto siga abierto.
