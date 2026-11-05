# Pruebas de argumentos y cancelación

Se añaden comprobaciones del orden de argumentos C y C++, recursos, inclusiones
y macros. También se comprueba que un sistema de archivos en memoria responda
antes de cancelar y devuelva `operation_canceled` en consultas posteriores.

Estas pruebas se ejecutarán con el resto de la tanda 04. La comprobación de la
capa de archivos no afirma que se interrumpa inmediatamente trabajo de CPU.
