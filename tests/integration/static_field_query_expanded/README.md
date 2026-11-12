# Controles propuestos para selección de campos

Estos casos conservan los ejemplos anteriores. Cada consulta tiene un control
ordinario independiente para distinguir sintaxis o visibilidad general del
comportamiento de `Fields`. Los resultados esperados de las consultas son los
del contrato deseado, no una afirmación de soporte actual.

- `generic_struct` y `generic_class`: dos campos de `Box<int>`, uno
  dependiente del parámetro y uno fijo; la suma debe ser 10.
- `private_type_inside`: el uso ocurre dentro de `Outer`, donde tanto el
  tipo privado anidado como el tipo del campo son accesibles; suma 10.
- `binary`: productor separado con dos tipos de campo, más consumidores con
  y sin consulta. Deben usarse la biblioteca y metadata del productor aislado.
- `binary_nominal`: campo de struct público con alias y homónimo en el
  consumidor; también productor separado con tipo interno inaccesible.
- `binary_generic`: productor genérico y consulta de la especialización
  concreta, con control ordinario.
- `metadata`: productores y consumidores para observar poda, campos privados,
  propiedad automática y backing; el nombre del consumidor no demuestra por sí
  mismo si la superficie fue podada o si se cargó un snapshot.

Los rechazos actuales por selección incompleta deben quedar registrados como
tales con diagnóstico; no equivalen a éxito ni demuestran que el caso ordinario
funcione si ese control no se ejecutó. No se registran aún en CMake.
