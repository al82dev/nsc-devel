# T08-004: biblioteca precompilada con globales C atómicas internas

El productor NativeSharp utiliza globales internas atómicas de varios tipos,
incluidas TLS y apuntados a record, enum y const. El consumidor llama al método
compilado mientras el encabezado C del productor está oculto. La matriz se
prepara para O0–O3; todavía no se ha ejecutado.
