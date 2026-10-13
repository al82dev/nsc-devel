# NS304-T09-F03 — Backing field sintetizado con layout visible

- **Estado**: CERRADO (sintetizado con nombre determinista + layout de campo regular)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): la property auto (`{ get; set; }` / `{ get; init; }`) baja
  su backing field como `FieldDeclaration` con nombre determinista
  `__nsc_backing_<name>$stage38` y metadata `property-backing:<name>`. El evento usa
  `__nsc_event_<name>$stage42` / `event-backing:`.
- **Member model** (`ProjectMemberModel.cpp`): el backing field se reconoce como miembro
  compilador-generado (`isCompilerGenerated`) con `associatedName`; su layout (offset/tamaño) lo
  computa la misma maquinaria de layout de cualquier campo, por lo que es visible y estable.
- Cobertura: `TL-LANG-334` (`{ get; init; }` auto-property), `TL-LANG-324`/`330` (extension
  properties), `TL-LANG-301..303` (properties/structs) ejercen el backing field.

## Nota

El requisito queda satisfecho por la síntesis existente; no requería cambios de código. Se registra
para cerrar el ítem y dejar evidencia en el tracking.
