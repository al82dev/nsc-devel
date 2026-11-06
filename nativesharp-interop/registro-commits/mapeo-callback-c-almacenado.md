# Mapeo compartido de punteros a función almacenados

Se expone el proyector ya usado para prototipos de funciones como entrada para
campos y globales C. Se conserva const del objeto almacenado y se rechazan
volatile y prototipos que el mapeador existente no puede representar. La
conexión a campos y globales, la construcción y las pruebas aún faltan.
