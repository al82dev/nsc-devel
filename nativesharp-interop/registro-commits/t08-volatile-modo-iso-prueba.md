# Tanda 8: prueba de modo volatile ISO

Se amplía la matriz existente con un proyecto que solicita `VolatileMode="ISO"`. La referencia Clang se compila con `-fno-ms-volatile`, el verificador exige accesos volatile ordinarios y ausencia de `atomic volatile`, y se prepara la ejecución O0–O3 junto con la variante predeterminada.

La opción y su validación se registran por separado. Esta prueba cubre accesos directos de la matriz, no la propagación por locales o parámetros.
