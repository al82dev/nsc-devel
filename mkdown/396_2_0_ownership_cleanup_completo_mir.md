# 396 — Ownership y cleanup completamente en MIR / Ownership and cleanup fully represented in MIR

El MIR OOP 1.9 publica por función un contrato ordenado de cleanup. Cada región tiene identidad,
clase (`local-owner`, `temporary-owner`, `owned-parameter`, `using` o `finally`), almacenamiento,
tipo, ownership y la acción MIR exacta que debe ejecutarse. / OOP MIR 1.9 publishes an ordered
per-function cleanup contract. Every region carries its identity, class (`local-owner`,
`temporary-owner`, `owned-parameter`, `using`, or `finally`), storage, type, ownership, and the
exact MIR action to execute.

El productor MIR rechaza identidades duplicadas y salidas estructuradas que mencionen regiones
inexistentes. LLVM deja de recorrer el árbol para redescubrir acciones: consume exclusivamente el
contrato ya validado. / The MIR producer rejects duplicate identities and structured exits that
reference missing regions. LLVM no longer walks the tree to rediscover actions; it exclusively
consumes the validated contract.
