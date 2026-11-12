# Contexto de análisis de bindings de nombres

El análisis de cuerpos utiliza shards por callable. El modelo anterior no
conservaba en el merge el contexto de lambda de cada binding de nombre, por lo
que no se podía distinguir de forma fiable una visita de nivel superior de una
visita en una lambda anidada. La ausencia del mapa provisional no demuestra
que un binding sea de nivel superior.

El recorder guarda el ordinal activo de lambda como candidato y deja el
contexto Unknown hasta fusionar el shard. El merger valida owner, scope,
diagnósticos, unicidad y owner de la lambda, rango y overflow; después remapea
el ordinal usando `lambdaBase`. Las visitas del mismo syntax/owner con scopes,
contextos o elegibilidad incompatibles quedan Unknown. Si el índice detecta el
mismo syntax/owner en otro shard, se invalidan los contextos de ambas partes;
el orden de bindings y la política newest-binding-wins se conservan. Este
metadato representa una visita de análisis, no ejecución, validez de ownership
ni seguridad de FnOnce; no modifica diagnósticos ni desactiva barreras.

La unidad dirigida de merger verificó el mapeo de lambdas anidadas y hermanas
en dos callables con uno y cuatro workers, bindings canónicos y controles
sintéticos de conflictos, owner/scope y preservación del índice. La ejecución
coordinada pasó 86 comprobaciones, observadas en consola sin log persistido; la
compilación terminó sin warnings en
`A:\nsc-vistas-evidencia\binding-context-index-build.log`. La compilación se
realizó con otros cambios de FnOnce presentes en el árbol de trabajo; no
demuestra por sí sola el resultado de compilar un commit selectivo aislado.

El test de duplicados entre shards ejercita el helper y el índice reales de
forma sintética. No demuestra un merge end-to-end que produzca el duplicado ni
que el scheduler lo genere. Con duplicados el merger puede recorrer el destino
completo repetidamente; no se ha acotado su costo agregado. Tampoco se ha
descartado que una vía diferente registre una única visita de syntax de lambda
fuera de su guard; el contexto no debe usarse para certificar seguridad hasta
demostrar esa frontera. La interoperabilidad C/C++ está fuera de esta tanda.
