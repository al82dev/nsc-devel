# Corrección de identidad HIR atómica

La construcción agrupada detectó que el indicador se había insertado en la
función de serialización de vectores, donde no existe `node`. Se coloca entre
los atributos del nodo HIR, que es la identidad que debe diferenciar.
