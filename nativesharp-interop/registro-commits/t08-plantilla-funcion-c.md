# Función C requerida por plantilla precompilada

El productor publica una plantilla que invoca una función C y usa su resultado.
`plantilla-funcion/validar.cmd` construyó la biblioteca C, el productor y un
consumidor con el encabezado; ambos lenguajes devolvieron el resultado esperado.
Después retiró la copia temporal del encabezado, compiló el consumidor sin
referencia a él y lo ejecutó con el mismo resultado. La prueba pasó en O0 con
el compilador de desarrollo y Clang en A:. La biblioteca C permanece disponible
para el enlace; no se afirma cobertura de otras firmas u optimizaciones.
