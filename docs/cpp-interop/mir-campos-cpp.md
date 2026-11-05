# Campos estáticos C++ en MIR

La construcción de MIR conserva los calificadores y el enlace de campos C++ y
usa su símbolo externo. Una declaración sin símbolo se rechaza y el estado con
error de importación no produce un campo MIR.
