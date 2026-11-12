# Await de una fábrica de Future

Una llamada directa `await Make()` fallaba durante la generación de código si
`Make` era síncrona y devolvía un Future creado por otra función. El compilador
intentaba buscar `Make$poll`, aunque el poll pertenece al future devuelto. La
misma operación mediante una variable local compilaba y devolvía el resultado.

El HIR sólo selecciona directamente el poll de un miembro marcado async. Para
una fábrica síncrona deja que el backend obtenga el poll del future, como ya
ocurría con variables. La expresión de la fábrica se sigue evaluando una vez.
La corrección no cambia el contrato de cancelación o Drop.

La reproducción previa y sus comandos están en
`A:/nsc-fixes-evidencia/async-finally-20261111/await-factory-minimal.log`: el
caso directo falló en O0/O2 y los controles por variable y llamada async
devolvieron 42. Después del build incremental, el runner dirigido
`tests/integration/async_future_factory.py` pasó 16/16 ejecuciones O0/O2 con
dos productores, fábrica de instancia, genérica, captura y suspensión, y
await síncrono de un future que queda listo en su primer poll, además de una
fábrica declarada en namespace. Evidencia:
`A:/nsc-fixes-evidencia/async-finally-20261111/await-factory-regression.log`.
El compilador de esa tanda tuvo SHA-256
`ebf688ae775c5ebb7bcfb83fd1e06cdf9d0dbced90529a497f37c830b9e0a184`.
El build incluía una ampliación de procedencia de vistas aún sin confirmar;
esta evidencia demuestra el comportamiento de la combinación, no del diff
de await aislado. Los controles de cancelación/abandono dieron 18/18 y la
prueba de namespace conservó los dos fallos SEH conocidos.

El control exploratorio con `Future<int, Exception>` no compiló por resolución
del tipo, así que no acredita propagación de error desde una fábrica. También
quedan sin verificar el dispatch virtual, módulos binarios y el await durante
cancelación; la corrección de este caso no implica que esos escenarios pasen.
