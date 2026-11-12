# Tipos de campo complejos en plantillas binarias

Los productores separados prueban tuplas fijas y dependientes de `T` y un
puntero a `T`. Los controles ordinarios comprueban uso normal; `Query` consulta
`Name` y `QueryTyped` también usa `field.Type` y `Get`, sin desreferenciar el
puntero. La matriz binaria espera que las tres formas compilen y ejecuten.
Los negativos de tipos inaccesibles y herencia genérica se prueban en sus
fixtures respectivos. Las alteraciones artificiales de certificados se
registran por separado de los programas de usuario.
