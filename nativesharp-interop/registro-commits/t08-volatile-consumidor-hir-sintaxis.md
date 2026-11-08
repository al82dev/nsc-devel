# T08-010/015: consulta del modo por expresión

El modelo HIR puede resolver el modo asociado a una expresión mediante el
`FileId` de su rango. Sintaxis nula, rangos inválidos y archivos sin selección
devuelven ausencia, sin suponer modo Microsoft o ISO. La consulta reutiliza el
mapa del modelo principal cuando se ejecuta en workers paralelos.

La función aún no se usa para cambiar accesos y no determina por sí sola si un
puntero designa almacenamiento C. La protección de generación sigue activa;
no se ha construido ni validado el conjunto.
