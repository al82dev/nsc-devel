# Tanda 8: conexión preparada de alias en el productor C

Cuando se solicita explícitamente el formato de metadata preparado 46, el
productor recopila los alias C de plantillas públicas que ya fueron resueltos y
los entrega al catálogo de dependencias. Un catálogo sin tipos puede contener
esas dependencias. Los formatos anteriores conservan su ruta original.

La construcción normal todavía no solicita ni publica el formato 46. La
captura tampoco cubre cuerpos genéricos sin binding, funciones o globales. No
se afirma interoperabilidad funcional ni cierre del circuito productor–consumidor.

Se construyó incrementalmente `nsc_cpp_c_identity_tests` y la ejecución terminó
con código 0, incluyendo el control de referencia inexistente de `f47647b0f`.
No hay todavía prueba positiva del productor con un proyecto que incluya una
referencia C real. La compilación mostró advertencias de inicializadores en
código existente.
