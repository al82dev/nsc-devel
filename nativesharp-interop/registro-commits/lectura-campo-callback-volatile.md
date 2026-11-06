# Lectura de campo callback volatile

Se prepara `acquire` en la lectura del campo cuando el modo MS y la alineación
efectiva permiten el acceso atómico al puntero. Si no hay esa evidencia se
conserva la lectura volatile ordinaria. Falta comprobar el IR y la ejecución.
