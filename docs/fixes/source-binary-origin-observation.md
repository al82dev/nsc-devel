# Observación de ordinales de unidades fuente y binarias

El experimento `tests/integration/source_binary_unit_origin_observe.py` se
ejecutó desde `A:/nsc-devel-fixes-agent-1-lambda`, con las cuatro variables de
temporales y datos de aplicación en A desde el lanzador y cada hijo. No se
ejecutó el producto compilado ni se cambió la metadata del producto o la
barrera de vistas binarias. Comandos, códigos y hashes están en
`source-binary-origin-observe-20261112.log`; fuentes y volcados íntegros en
`nsc-unit-origin-observe-x9l7ug3g/`, ambas rutas bajo esta carpeta.

El productor fuente `OriginProducer` tiene dos unidades con ordinales de
proyecto 0 y de fuente 0/1; el volcado de nombres muestra `Piece` con dos
partes. La biblioteca O0 se emitió correctamente: su SHA-256 es
`c917ac93daadbd36f4184144a0223a3bbf21e9b3968abcf963c9787e522002c7`
y el de su metadata
`cbb107693a27512d0758afa25e95847aee55d573583070f6e0058c83a97cbfa7`.

El consumidor binario mostró la fuente `OriginConsumer` en proyecto 1, y
dos unidades derivadas de metadata de `OriginProducer` en proyecto 0: API con
source 0/file-id 2 y templates con source 1/file-id 3. El volcado de nombres
mostró la clase binaria `Piece` como una parte en módulo OriginProducer, y
`Box<int>` especializado en ese mismo módulo con source 1/file-id 3. El
volcado de genéricos reportó una especialización materializada y un cache hit.
Los volcados se hicieron en procesos separados: coincidencias de ordinales y
file-id entre ellos son observaciones, no una identidad verificada entre
procesos ni una prueba de procedencia auténtica de una especialización.

El intento de aportar una parte fuente bajo el mismo nombre de módulo que la
biblioteca falló con `source-preferred project reference introduces a cycle at
module 'OriginProducer'`. El grafo se volcó, pero compilación, nombres y
genéricos de ese caso devolvieron 1; no se produjo un tipo mixto y su
procedencia permanece Unknown. No se probó si otra configuración permite
partes mixtas, ni restauración, cachés adicionales, colisiones o mutaciones.

El ejecutable nsc tuvo SHA-256 estable
`6866e0d770e46bdb7ad29d8297d6a846d083a1f72dc96d58a5c4aa1e207991eb`.
Los ordinales observados no demuestran ausencia de escapes, destructores,
efectos implícitos ni integridad de un nodo o metadata; no justifican publicar
un contrato Verified ni retirar la barrera conservadora.
