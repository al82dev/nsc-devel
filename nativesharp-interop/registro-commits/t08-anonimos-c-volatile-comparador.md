# Comparación de IR volatile en agregados C anónimos

El fixture prepara getters y setters equivalentes en C y NativeSharp para los
campos naturales y empaquetados. El validador reutiliza el comparador de la tanda
para contrastar orden, volatilidad, tipo y alineación contra Clang en IR O1–O3,
en directo, dos sesiones, Shared y Metadata.

El comparador ampliado aún no se ha ejecutado. La tanda 8 sigue abierta.
