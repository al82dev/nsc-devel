# T08-003: activar sidecars con objetos nativos

Los productores ThinLTO publican el sidecar cuando tienen bitcode, incluidos
los bundles con objetos nativos generados. El escritor usa v3 para esos objetos;
el consumidor preparado restaura los objetos junto a sus unidades optimizadas y
conserva definiciones durante el backend. Los sidecars sin objetos siguen en v2.

La activación está pendiente de construcción y pruebas agrupadas. No se afirma
compatibilidad integral ni cierre de la tanda hasta disponer de esos resultados.
