# Cierre de dependencias de records C

Se recopilan recursivamente records solicitados y referidos desde sus campos,
incluidos tipos de función anidados. Las referencias enum se devuelven aparte
para su posterior persistencia. Se rechazan identidades ausentes, ambiguas o
USR conocidos incompatibles, y se conserva el catálogo ordenado. Aún no está
conectado a la publicación ni validado por ejecución.
