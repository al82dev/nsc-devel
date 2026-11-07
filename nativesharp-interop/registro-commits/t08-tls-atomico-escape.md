# Barrera de dirección para almacenamiento C atómico

El análisis preparado impide obtener la dirección de un global C atómico o
pasarlo por `ref`, `out` o `in` mientras el sistema de tipos no represente
punteros atómicos. La marca todavía no se proyecta, por lo que este cambio no
habilita accesos ni sustituye el soporte completo de escapes.
