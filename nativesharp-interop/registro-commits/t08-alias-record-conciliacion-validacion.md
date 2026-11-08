# Validación de conciliación de alias a records

La construcción incremental de `nsc` con `d1b574522` terminó con código 0
(`c04d0d0299ecbb02`) y tres advertencias de inicializadores en
`CppSemanticRecord.cpp`. CMake regeneró la configuración y la construcción
programó 39 pasos incrementales.

`validar-referenciados.cmd` terminó con código 0 de O0 a O3: un consumidor
utilizó el alias y el enum publicados con el encabezado C oculto.
`pruebas-tanda-08-anonimos-c/validar.cmd` terminó con código 0 de O0 a O3,
incluidos el encabezado compartido, el consumo por metadata y el control de
definiciones incompatibles. Estas pruebas no acreditan alias de otros tipos,
funciones de namespace ni la publicación de nombres ajenos a la API. No se
ejecutó el gate completo de la tanda 8.
