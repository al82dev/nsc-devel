# T08-010/015: modo por unidad en HIR

La construcción de HIR prepara una asociación entre `FileId` y el modo
opcional del proyecto dueño de cada unidad. La consulta de un worker paralelo
usa el mismo mapa del modelo principal; un archivo sin modo permanece ausente.

Este paso no aplica todavía el modo a accesos ni resuelve el origen de punteros.
La protección temporal contra configuración explícita permanece activa. Falta
comprobar unidades precompiladas, genéricos, identidad de caché y la generación
LLVM antes de quitar esa protección. No se ha construido ni ejecutado esta
modificación; se validará con el conjunto en curso.
