# T08-010/015: alineación comprobable del puntero flexible

Para un miembro flexible C apto para conversión a puntero, el análisis calcula
la alineación a partir de la garantía del puntero receptor, el desplazamiento y
la alineación efectiva importada del campo. Si falta cualquiera de esos datos o
el receptor no es un puntero, conserva la garantía mínima de un byte. No toma
la alineación natural del tipo como prueba suficiente.

La alineación inferida forma parte del contrato existente de los punteros
locales; las asignaciones deben satisfacerlo. El modo explícito sigue protegido
y este cambio no afirma corregir todos los accesos. Se construirán y validarán
conjuntamente la propagación, las reasignaciones y los controles HIR.
