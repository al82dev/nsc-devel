# Prueba de cierre de dependencia con USR distinto

Una compilación usa el adaptador de sesión de prueba que altera la identidad
de `Branch.items`. El selector de dependencias debe rechazar el record
referenciado y describir la discrepancia de USR. El fixture positivo anterior
conserva la sesión original. La ejecución espera la batería agrupada.
