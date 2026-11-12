# Base binaria para construcción compartida: propuesta parcial

Base de la propuesta: `2cb01ab30878dee57ee74be9b28c6f99ec828f38`.
El código y el fixture están integrados como trabajo en curso; no se han
compilado ni ejecutado después de su integración.

La destrucción de un subobjeto de base importada utiliza el accessor del
descriptor publicado por el productor y la entrada de destrucción del objeto
en la ranura 7. Esa entrada destruye el subobjeto sin liberar el bloque de la
fábrica. La bandera pasa a viva únicamente después del retorno de la llamada.

El guard limita la aceptación a una base cuyo productor aportó el contrato
existente de no escape, con descriptor accesible y una llamada identificada a
un constructor binario declarado no lanzador. Para una base trivial se
conserva la regla anterior. Un constructor binario que puede lanzar sigue
rechazado: el contrato de no escape no prueba que limpie sus campos cuando
falla. No se ha incorporado un nuevo contrato de metadata.

`tests/integration/shared_throwing_imported_base.py` prepara un productor
NativeSharp y consumidores que ejercitan Rc y Arc exteriores, las cuatro
familias en los campos, constructor delegado, fallo y éxito en O0 y O2.
Incluye un negativo con constructor binario declarado `throws`. El fixture
recibe compilador, Core, metadata y carpeta de evidencia explícitos en A.

Quedan sin comprobar la compilación, ejecución, identidad y procedencia de los
artefactos, compatibilidad con metadata existente, liberación física del bloque,
base binaria de constructor lanzador y cleanup cuando un destructor lanza.
El uso de la entrada del descriptor no demuestra por sí solo esos extremos.
