# Base binaria para construcción compartida: propuesta parcial

## Validación posterior

En la continuación posterior a `3f8287fa0`, con el compilador incremental
aislado `A:/nsc-fixes-evidencia/build/bin/nsc.exe` (build hash
`5f77cbcea6996489`) y Core binario de `integracion-vistas-sdk`, volvió a pasar
la matriz `shared_throwing_imported_base.py`: 16 ejecuciones de productores y
consumidores O0/O2 para Rc y Arc, además del rechazo esperado de la base cuyo
constructor declara `throws`. Comandos y salidas están en
`A:/nsc-fixes-evidencia/shared-imported-base-current-20261113.log`.
En la misma configuración, `shared_nontrivial_base_throw.py` pasó los casos
de bases fuente, delegación, errores y recuperación para Rc y Arc en O0/O2;
el comando y la salida son `nontrivial-base-current-20261113.cmd` y `.log`.
Este último runner registra los resultados pero no conserva sus proyectos
temporales. Estas pruebas no demuestran limpieza de una base binaria lanzadora
ni liberación física del bloque compartido.

En la revisión `a373d3671`, el runner compiló productores O0/O2 y ejecutó
16 consumidores O0/O2, para Rc y Arc, constructor normal o con excepción en
la clase derivada: todas las ejecuciones devolvieron 42. El negativo de base
binaria declarada lanzadora fue rechazado por la guarda esperada. Comandos y
salidas: `A:/nsc-fixes-evidencia/shared-imported-base-throws-fix-20261112.log`;
fuentes y artefactos generados en la carpeta homónima. El compilador aislado
tuvo SHA-256 `da0145daa255ab8f13031aa85dba1af05e4c995436ee29f2ba84b3b11a595e1e`.

La primera repetición sobre `57eab24a8` ejecutó los 16 positivos, pero aceptó
el negativo. Se identificó y corrigió una pérdida del contrato `throws` al
reconstruir el constructor binario; detalles y prueba separada en
`binary-constructor-throws-contract.md`. Conservar esa evidencia no convierte
la base binaria lanzadora en admitida: sigue rechazada hasta demostrar la
limpieza parcial del productor. Tampoco se acreditan liberación física,
compatibilidad de otras bibliotecas ni destructores que lanzan.

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
