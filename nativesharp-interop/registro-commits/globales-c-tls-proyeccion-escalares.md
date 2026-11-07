# Proyección de escalares TLS de C

La prueba de control compilada como C pasó, pero NSC rechazó la proyección
de los seis globales de su cabecera. Se amplía la proyección TLS estática de
C a los escalares que el mapeo existente ya puede representar, incluyendo
`const`. Para `volatile` solo se habilita `int` de 32 bits, cuyo tratamiento
de accesos ya existe; los demás siguen rechazados hasta contar con evidencia.

La compilación y las pruebas de esta modificación quedan pendientes hasta la
validación agrupada. Esto no habilita TLS de DLL ni modifica NSLink.
