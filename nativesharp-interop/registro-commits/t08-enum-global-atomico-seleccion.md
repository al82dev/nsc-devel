# T08-004: dependencia enum dentro de una global atómica

La observación de tipos requeridos recorría punteros y arreglos, pero se
detenía en Atomic. Por eso una global atómica cuyo valor apunta a enum no
incorporaba la declaración enum al proyecto. Se incluye el elemento de Atomic
en el mismo recorrido acotado. Pendientes construcción y validación.
