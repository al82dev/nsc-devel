# Tanda 08: carga de typedef antes del parser

Cuando está configurado, `ProjectCompilation` solicita el inventario de aliases
con el grafo preparado y conserva las observaciones durante la compilación.
Valida que cada ordinal y archivo de proyecto correspondan antes de aceptarlas.
Este paso todavía no añade los nombres al parser ni habilita el proveedor desde
el driver. Su validación se hará junto al resto de la tanda.
