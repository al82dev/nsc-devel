# Validación con encabezado C ausente

El fixture `pruebas-tanda-08-anonimos-c/validar.cmd` terminó con código 0. En
cada nivel O0–O3 el consumidor Metadata se compiló y ejecutó mientras la copia
de `Api.h` usada por el productor estaba renombrada fuera de su ruta original.
Usó los agregados anónimos y la plantilla que depende de un enumerador C.

Al terminar se comprobó que `Api.h` quedó restaurado y que `Api.hidden` no quedó
en el directorio temporal. El archivo original del repositorio no fue renombrado.
La batería completa de la tanda 8 sigue pendiente.
