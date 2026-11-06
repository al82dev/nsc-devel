# Reconstrucción inicial de tipos C

Se preparan declaraciones importadas de records y enums para un módulo y
proyecto explícitos. La reconstrucción comprueba identidad, destino, dimensiones
y tipo subyacente, y distingue layout por valor de acceso por puntero. No instala
campos ni constantes y aún no está conectada al consumidor ni validada.
