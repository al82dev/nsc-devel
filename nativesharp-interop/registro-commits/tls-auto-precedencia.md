# Controles adicionales de selección automática TLS

Se amplía la matriz con una biblioteca C estática sin TLS: su proyecto debe
conservar NSLink y ejecutarse. También se comprueba que `NSC_LINKER` explícito
con NSLink prevalece ante el indicio TLS, además de los controles de opción y
sidecar. La matriz inicial pasó el caso TLS interno O0–O3, el consumidor
precompilado, el proyecto sin referencias C y las selecciones por opción y
sidecar con NSC build `2c3c6e700529d377`. La ejecución ampliada pasó también
los controles de biblioteca C sin TLS y `NSC_LINKER` explícito.
