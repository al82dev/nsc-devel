# Atributo atómico en estado y HIR

Se prepara un indicador propio para almacenamiento C atómico en el inventario,
estado de namespace y nodo HIR de acceso, separado de `volatile`. Aún no se
proyecta ningún global atómico: el indicador permanece falso hasta conectar la
generación segura y la validación. Falta transporte MIR y su caché.
