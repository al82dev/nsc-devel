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

La prueba real ya confirmó el fallo: el productor de O0 generó la biblioteca,
pero el consumidor rechazó `CFloatRecord.value` al reconstruir el catálogo.
Una solución debe mantener la lectura canónica de catálogos anteriores y
transportar el formato también dentro de punteros, arreglos y firmas. El dato
debe participar en la comparación de contratos y en el fingerprint de ABI;
no debe incluir la ruta o huella de procedencia en ese fingerprint. Se
necesitan controles de lectura antigua, truncamiento, alteración de indicador,
formatos incompatibles y consumidor real, además de la matriz O0–O3.
