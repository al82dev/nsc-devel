# Construcción de registros C++ en MIR

La bajada MIR consulta el tamaño y alineación del layout importado y exige un
constructor C++ seleccionado con argumentos de las formas admitidas por el
código actual. Los demás casos producen error y requieren trabajo posterior.
