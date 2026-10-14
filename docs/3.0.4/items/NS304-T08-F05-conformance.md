# NS304-T08-F05 — Conformance externa (orphan/coherence)

- **Estado**: CERRADO (orphan rule + conformance duplicada + dispatch intermodular + witness NSMETA)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar conformance externa*.
Plan T08-F05 (DeepSeekDocs): *congelar orphan/coherence: definir qué módulo posee tipo, trait y
extensión; detectar dos impls visibles, ciclos y conformance retroactiva incompatible; publicar
witness y constraints en metadata para resolución intermodular*.

Estado real:

- **TraitSemantics**: `Language/TraitSemantics.*` tiene witnesses, coherence y conformances (la
  resolución de conformance nominal ya existe).
- **Orphan rule**: sin definición congelada de qué módulo posee tipo/trait/extensión (para la
  conformance EXTERNA, no nominal).
- **Metadata**: sin publicación del witness + constraints del operador extension.

## Gap

Falta congelar la regla orphan/coherence para la conformance externa (dos impls visibles, ciclos,
conformance retroactiva incompatible) y publicar el witness + constraints en NSMETA.

### Verificación (2026-10-13)

- La conformance nominal `impl Trait for Class` **funciona** (exit 5); el impl duplicado se rechaza
  por firma (`duplicate method signature`). El target debe ser **clase no estática** (un struct da
  `trait impl target must be a non-static class`).

### Verificación (tanda 227-231)

- **Orphan rule** (TL-LANG-403): `impl Local for External` con trait poseído por el módulo compila;
  `impl ExternalTrait for External` (no posee ni tipo ni trait) se rechaza con `orphan impl`.
  (`ProjectNameLookup::finalizePredeclarations`, coteja `projectOrdinal` del impl contra el del trait.)
- **Conformance duplicada**: `impl Addable for Vector` dos veces → `duplicate trait impl` (ya existía).
- **Dispatch intermodular** (TL-LANG-404): `impl Local for External` + `e.F()` → **exit 5**. Los
  trait-impl parts sobre targets externos se almacenan en `ProjectNameLookup::externalTraitImplParts_`
  y `ProjectMemberModel` registra sus métodos en la superficie del tipo externo.

### Pendiente

- **Witness + constraints NSMETA**: serializar el record de conformance (target + trait + impl
  methods) y reconstruirlo en un consumidor de 3 módulos (`e.F()`=5 a través de NSMETA). La
  serialización sola sería superficie desconectada; va acoplada a la reconstrucción del consumidor.

### Verificación (witness NSMETA, TL-LANG-406)

- NSMETA 1.40 añade `LibraryMetadataTraitImpl` (target + trait + impl methods). El productor lo
  publica desde `externalTraitImplParts_` y el consumidor lo materializa como `impl Trait for Type`
  en la fuente pública; el orphan rule + el registro de miembros reconstruyen el dispatch.
- Tres módulos (tipo externo + impl + consumidor sin fuentes del impl) → `e.F()` **exit 5**.
- Lo pendiente es el **vertical de reglas** (orphan: posesión de módulo del tipo/trait/extensión) +
  la **metadata NSMETA** (witness + constraints), no una surface nueva.

## Nota

F05 es la coherence de la conformance externa (operador extension, F04). Es un vertical de reglas +
metadata, no una surface nueva; depende de F04 (witness) y de la definición de posesión de módulo.
