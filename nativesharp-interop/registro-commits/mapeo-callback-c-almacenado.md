# Mapeo compartido de punteros a función almacenados

Se expone el proyector ya usado para prototipos de funciones como entrada para
campos y globales C. Se conserva const del objeto almacenado y se rechazan
volatile y prototipos que el mapeador existente no puede representar. La
conexión a campos y globales se construyó y los casos dirigidos pasaron
O0–O3; sus controles de escritura const se rechazaron correctamente. Otras
firmas y almacenamiento volatile siguen pendientes.
