# Ampliación de la prueba de union C

El fixture preparado comprueba lecturas y escrituras cruzadas entre C y
NativeSharp, incluidos los bytes superpuestos y el entero del mismo objeto en
Windows x64. El script compila y ejecuta el consumidor en O0–O3. Pendiente de
ejecución agrupada; no cubre union por valor ni todos sus miembros posibles.
