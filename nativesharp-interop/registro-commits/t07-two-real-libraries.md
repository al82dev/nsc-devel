# Tanda 07: dos bibliotecas reales con configuraciones independientes

El escenario prepara dos bibliotecas C separadas con Clang y cada header exige
un valor diferente de la misma macro. NSC debe abrir ambas referencias con sus
opciones propias, enlazar las dos bibliotecas y ejecutar el consumidor.
Pendiente de ejecución agrupada; no demuestra todas las bibliotecas C o C++.
