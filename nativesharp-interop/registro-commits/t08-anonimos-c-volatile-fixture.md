# Campos volatile en agregados anónimos C

El fixture incorpora campos `volatile int` promovidos desde structs anónimos en
records natural y `pack(1)`. NativeSharp los escribe y lee; C comprueba los valores.
Está pendiente la ejecución y la comparación de orden y alineación en IR.
