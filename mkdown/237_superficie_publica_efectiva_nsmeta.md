# 237 — Superficie pública efectiva de NSMETA / Effective NSMETA public surface

## Resultado / Outcome

**ES.** NSMETA parte de los tipos del proyecto raíz cuya cadena completa de contenedores es pública
y conserva sólo sus dependencias contractuales. Un miembro `public` dentro de una clase `internal`
inconexa deja de convertirse en una obligación ABI inexistente para consumidores y verificadores.

**EN.** NSMETA starts from root-project types whose complete containing-type chain is public and
retains only their contract dependencies. A `public` member inside an unrelated `internal` class
no longer becomes a nonexistent ABI obligation for consumers and artifact verification.

## Causa / Cause

El constructor del modelo reunía todos los tipos pertenecientes al proyecto raíz antes de calcular
accesibilidad efectiva. La comprobación de la cadena pública sólo se usaba para decidir si un
callable necesitaba símbolo físico; los registros de tipo y miembro ya se habían agregado.

The model builder collected every type owned by the root project before computing effective
accessibility. The public-chain check was used only to decide whether a callable required a
physical symbol; type and member records had already been added.

## Implementación / Implementation

- Se conserva un conjunto de todos los tipos del proyecto raíz para validar pertenencia.
- La raíz pública se amplía únicamente con tipos nombrados por herencia, layout o firmas públicas;
  esto conserva contratos como un `extern C delegate` sin publicar helpers internos inconexos.
- Miembros, dispatch y símbolos físicos siguen exigiendo una cadena de contenedores `public`.
- No cambia el layout interno del binario; cambia únicamente el contrato exportado.

All root-project types remain available for ownership checks. Publication starts with types whose
complete containing chain is `public`, then adds only types referenced by public inheritance,
layout, or signatures. This preserves contracts such as an `extern C delegate` without publishing
unrelated internal helpers. Members, dispatch, and physical symbols still require public
containers. Binary-internal layout is unchanged; only the exported contract changes.

## Archivos / Files

- `src/Interop/LibraryMetadata.cpp`
- `mkdown/237_superficie_publica_efectiva_nsmeta.md`

## Verificación dirigida / Targeted verification

Se usa el caso `InternalTypePublicField` de Stage 48.2 y se reconstruye el compilador desde cero.
Los otros diagnósticos independientes de Stage 48.2 se atienden en entregas separadas. No se
ejecuta baseline intermedio.

The Stage 48.2 `InternalTypePublicField` case is used, followed by a clean compiler rebuild. Other
independent Stage 48.2 diagnostics are handled by separate deliveries. No intermediate baseline is
executed.

## Commit de referencia / Reference commit

**Título ES:** Limita NSMETA a la superficie pública efectiva

**Title EN:** Restrict NSMETA to the effective public surface

**Descripción ES:** Excluye tipos y miembros cuyos contenedores no son públicamente alcanzables del
contrato precompilado.

**Description EN:** Exclude types and members whose containers are not publicly reachable from the
precompiled contract.
