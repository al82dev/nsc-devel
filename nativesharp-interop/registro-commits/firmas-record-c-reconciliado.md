# Firmas C con records reconciliados

Los grupos de funciones conservan los records Clang cuya identidad fue
reconciliada y proyectan su firma a la identidad canónica del catálogo. El
caso por valor mantiene las verificaciones de layout y ABI; la excepción de
propietario solo aplica a un ordinal reconciliado. Esto no implica
interoperabilidad completa.

La construcción incremental, la suite de sesión y el caso de encabezado
compartido de O0 a O3 pasaron. Se rechazó el caso con layout modificado; quedan
otros contratos y la validación completa de la tanda.
