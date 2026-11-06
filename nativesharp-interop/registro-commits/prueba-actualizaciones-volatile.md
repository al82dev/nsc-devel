# Pruebas de actualización volatile de campo C

La matriz amplía los incrementos con resultados prefijo y postfijo y asignación compuesta. El control IR exige dos lecturas y una escritura para actualizaciones, como Clang, y reconoce la alineación menor y conservadora del campo de 8 bits. La ejecución sigue pendiente hasta correr el grupo.

Validado O0–O3: el resultado de prefijo y postfijo y la asignación compuesta coinciden con los valores esperados, y el control IR pasó para los casos de la matriz. Las regresiones nativas 329 y 330 también pasaron.
