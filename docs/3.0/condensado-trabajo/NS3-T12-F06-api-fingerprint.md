# NS3-T12-F06 — Huella de API pública / Public API fingerprint

## Español

La superficie pública tiene dos proyecciones canónicas, versionadas como
`NativeSharp.source-api.v2` y `NativeSharp.abi-surface.v2`. La primera describe lo que el
consumidor puede nombrar y comprobar: identidad nominal, accesibilidad pública/protegida, firmas,
ownership, effects, lifetimes, constraints transportados, traits/impls y cuerpos genéricos que el
consumidor debe materializar. La segunda describe decisiones físicas: tamaños, alineamientos,
campos, símbolos, calling ABI, especializaciones y dispatch publicado.

Ambas proyecciones usan campos delimitados por longitud y orden por identidad canónica. El orden
del archivo NSMETA, los miembros privados, los nombres de presentación y los comentarios/espacios
de plantillas no cambian la huella. Un cambio semántico del cuerpo genérico sí cambia la superficie
source; un cambio de layout o dispatch cambia la superficie ABI.

`nsc --dump-api-surface <file.nsmeta>` publica las dos proyecciones y sus SHA-256 para herramientas.
`--api-diff` usa exactamente los mismos bytes canónicos para clasificar cambios aditivos, source
breaking y ABI breaking.

## English

The public surface has two canonical projections, versioned as `NativeSharp.source-api.v2` and
`NativeSharp.abi-surface.v2`. The source projection describes what consumers can name and check:
nominal identity, public/protected accessibility, signatures, ownership, effects, lifetimes,
transported constraints, traits/impls, and reusable generic bodies. The ABI projection describes
physical decisions: sizes, alignments, fields, symbols, calling ABI, specializations, and published
dispatch.

Both projections use length-delimited fields ordered by canonical identity. NSMETA record order,
private members, display names, and generic comments/whitespace do not affect a fingerprint. A
semantic generic-body change does affect the source surface; a layout or dispatch change affects
the ABI surface.

`nsc --dump-api-surface <file.nsmeta>` exposes both projections and their SHA-256 values to tools.
`--api-diff` consumes the exact same canonical bytes to classify additive, source-breaking, and
ABI-breaking changes.
