# Persistencia pendiente del formato flotante

`CppType::ieeeBinary64` se captura en memoria, pero `putType` y `getType` del
catálogo C actual no lo escriben ni leen. El codec revalida su representación
canónica y el fingerprint usa el catálogo sin procedencia. Añadir el byte sin
versionar alteraría el formato y la compatibilidad de bibliotecas existentes;
además, la comparación de contratos no distinguiría formatos mientras omita el
dato. Debe diseñarse una versión compatible que preserve y compare la semántica
en tipos anidados, respete los lectores anteriores y se pruebe con productor y
consumidor reales. El fixture de biblioteca precompilada está preparado, no
validado. No se afirma soporte de este caso hasta resolver el contrato.
