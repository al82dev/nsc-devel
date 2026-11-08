# T08 H3: excluir usos locales comprobados de las demandas C

El observador produce candidatos sintácticos también para variables locales.
Antes de consultar proveedores y atribuir candidatos a plantillas, se identifican
usos no calificados que tienen una declaración local anterior en un bloque
contenedor. El cálculo considera listas de declaraciones y la posición exacta
del uso, conservando los inicializadores y las referencias fuera del bloque.
Cuando hay varias plantillas propietarias, se exige que todas las aplicables
demuestren la sombra. Los demás casos permanecen como candidatos; este cambio no
pretende resolver por sí solo parámetros, patrones ni todos los ámbitos.

Se actualiza el test de identidad para seguir comprobando que el observador
recoge candidatos, pero no publica el uso local ya demostrado. Falta compilar y
ejecutar los controles dirigidos y las regresiones de publicación C; la tanda 8
permanece abierta.
