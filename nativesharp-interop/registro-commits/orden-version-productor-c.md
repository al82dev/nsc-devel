# Versión antes de generar catálogo C

El productor seleccionaba la versión del catálogo antes de elevar la metadata;
por ello, aun con el compilador 1.43, las bibliotecas reales escribían 1.42 y
perdían la procedencia. Se presenta 1.43 al proveedor y se recupera la versión
anterior cuando no se produce catálogo. Pendiente de construcción incremental
y comprobación real; esto todavía no concilia tipos entre módulos.
