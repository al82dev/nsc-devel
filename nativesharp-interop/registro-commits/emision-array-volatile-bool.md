# Emisión de lectura y asignación de _Bool volatile C

Para el arreglo global C acreditado, la lectura atómica volatile usa i8 con
acquire y se convierte a bool i1. La asignación simple amplía el valor i1 a i8
y lo escribe con release. Los demás booleanos no cambian de ruta. Las
operaciones compuestas y la validación agrupada siguen pendientes.
