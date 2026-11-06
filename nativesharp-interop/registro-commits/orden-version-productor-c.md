# Versión antes de generar catálogo C

El productor seleccionaba la versión del catálogo antes de elevar la metadata;
por ello, aun con el compilador 1.43, las bibliotecas reales escribían 1.42 y
perdían la procedencia. Se presenta 1.43 al proveedor y se recupera la versión
anterior cuando no se produce catálogo. Construcción incremental y suite de
sesión pasaron. El productor real generó metadata que contiene `Api.h` y su
huella SHA-256; los consumidores de encabezado independiente pasaron O0–O3.
La variante de encabezado compartido falló en O0 con conflicto de alias; la
batería terminó con código 1. Esto no concilia tipos entre módulos.
