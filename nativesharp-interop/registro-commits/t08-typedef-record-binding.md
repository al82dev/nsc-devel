# Tanda 08: enlace inicial de alias a record

El enlace contrasta la identidad del record del `typedef` con los records
importados del proyecto de origen antes de expandir el nombre y preserva el
ordinal del proyecto. Si alias y etiqueta nombran el mismo record importado, se
permite continuar con el record; otras colisiones reciben diagnóstico. Esto no
habilita copia, construcción o paso por valor de records, y no está validado.
