# Extensión versionada de formato flotante

Este registro describe una etapa anterior a la validación. El resultado
posterior, incluidos el productor y consumidor precompilados en O0–O3, está
en [metadata-long-double-c.md](metadata-long-double-c.md).

El catálogo preparado conserva su envoltura anterior cuando no necesita el
indicador y utiliza una segunda variante con tabla de formatos para tipos
`long double`. El lector acepta ambas, limita y valida la tabla y exige
recodificación canónica. Falta conectar la versión de metadata, comparación y
fingerprint y validar compatibilidad y datos inválidos.
