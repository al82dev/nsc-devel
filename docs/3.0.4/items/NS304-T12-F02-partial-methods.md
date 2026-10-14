# NS304-T12-F02 — Métodos parciales

- **Estado**: PARCIAL (declaración sin body + body definitorio fusionados; firma completa/effects/attributes/constraints pendientes)
- **Rama**: development

## Resultado (firma + body — 2026-10-14)

- **Parser** (`parseClassMember`): `partial` ante tipo de retorno de miembro marca el callable
  `partial-method`; `parseMethodTail` acepta el bodyless parcial sin `abstract`.
- **Member model**: dos `partial-method` con la misma `signatureKey` se **fusionan** (el body se
  adjunta a la firma); dos bodies definitorios → *"partial method 'M' has more than one defining
  body"*; llamada sin body → *"Stage 35 concrete OOP callable has no body"*.
- Verificado: `partial int M();` + `partial int M(){ return 5; }` → `f.M()` exit 5. Registrado
  `TL-LANG-416` (PASS).

## Residual

- **Firma completa + emparejamiento normativo**: effects/defaults/attributes/generic constraints/
  ownership de las dos partes deben coincidir; hoy se emparejan por `signatureKey` (nombre+aridad+
  tipos) y no se valida la coincidencia del resto del contrato.
