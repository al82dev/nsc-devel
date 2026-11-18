# Retorno de campo prestado con lifetime coincidente

Una vista declaraba un campo `borrowed[a] T` y un método que retornaba
`borrowed[a] T`; `return campo` se rechazaba porque el análisis de esa lectura
no transmitía su calificador a la validación del retorno. La corrección acepta
ese patrón estrecho cuando el campo de instancia pertenece al mismo tipo y su
lifetime declarado coincide exactamente con el lifetime del retorno. Un campo
con un nombre de lifetime distinto no se acepta por esta vía.

La prueba `nsc.compiler.borrowed-field-return` ejecuta el uso válido genérico
en O0 y O2 con resultado 42, rechaza guardar el resultado prestado en un local
sin calificador y rechaza un lifetime diferente. También se ejecutaron las
pruebas dirigidas de vistas (`A:/nsc-fixes-evidencia/view-lifetime-tests.log`).
El ajuste no convierte un campo ordinario en préstamo ni demuestra que todas
las relaciones de lifetimes, retornos de campos anidados o referencias de
propiedades sean seguras. Las cuatro familias siguen pendientes.
