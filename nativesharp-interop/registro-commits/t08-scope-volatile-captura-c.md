# Alcance: modelo volatile en captura C

El proveedor de campos ya utiliza el modelo de volatile observado por cada
sesión. La captura y el codec deben conservar ese dato sin inventar un consenso
cuando discrepe, para no perderlo al reconstruir metadata en el futuro.
