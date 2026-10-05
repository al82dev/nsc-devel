# NS3-T12-F07 — Ownership genérico exportado / Exported generic ownership

## Español

NSMETA 1.38 agrega un catálogo semántico append-only por plantilla reutilizable. Cada parámetro
publica su clase (`type`, `const`, type pack o const pack), tipo de constante, constraints nominales
y de igualdad, y las capacidades `class`, `struct`, `new()`, `copy`, `move`, `lifetime/drop`,
`unmanaged` y `notnull`. La plantilla publica además binders lifetime borrados y aristas `outlives`.

El catálogo se deriva del AST y de las claves de tipo canónicas después del análisis semántico; no
se reconstruye raspando texto. El source materializable continúa dentro de NSMETA para que el
consumidor monomorfice con el mismo frontend, mientras el nuevo catálogo permite verificar,
comparar y auditar el contrato sin los archivos fuente del productor. El codec rechaza conteos,
flags, nombres, constraints duplicados y aristas lifetime inválidas. Un modelo que usa estos campos
no puede etiquetarse como 1.37.

La huella de API source incorpora el catálogo completo. La prueba puntual compila una biblioteca
O0, lee su NSMETA, compila un consumidor separado mediante `LibraryReference` y NSLink, ejecuta el
resultado 42 y comprueba que un consumidor incompatible conserva el rechazo del productor.

## English

NSMETA 1.38 adds an append-only semantic catalogue for every reusable generic template. Each
parameter publishes its kind (`type`, `const`, type pack, or const pack), constant type, nominal
and equality constraints, plus the `class`, `struct`, `new()`, `copy`, `move`, `lifetime/drop`,
`unmanaged`, and `notnull` capabilities. Templates also publish erased lifetime binders and
`outlives` edges.

The catalogue is derived from the analyzed AST and canonical type keys; it is not reconstructed by
scraping text. Materializable source remains embedded in NSMETA so the consumer monomorphizes with
the same frontend, while the explicit catalogue makes the contract verifiable, comparable, and
auditable without producer source files. The codec rejects invalid counts, flags, names, duplicate
constraints, and lifetime edges. A model using these fields cannot be mislabeled as 1.37.

The source API fingerprint includes the entire catalogue. The targeted test builds an O0 library,
reads its NSMETA, builds a separate `LibraryReference` consumer through NSLink, executes result 42,
and verifies that an incompatible consumer retains the producer's rejection.
