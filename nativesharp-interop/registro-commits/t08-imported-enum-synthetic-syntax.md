# Tanda 08: sintaxis de enum importado

`ProjectNameLookup` valida el tipo entero subyacente y construye un nodo enum
sintético poseído por el modelo. Así los consumidores que consultan la sintaxis
pueden obtener el ancho y signo informados por Clang sin recurrir al `int` por
omisión. La comprobación de identidad admite ese nodo solo para la entrada
importada correspondiente. Aún falta alimentar este contrato y validarlo.
