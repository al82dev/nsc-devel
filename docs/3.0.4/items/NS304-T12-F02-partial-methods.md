# NS304-T12-F02 — Métodos parciales

- **Estado**: IMPLEMENTADO (declaración sin body + body definitorio fusionados; firma completa = refinamiento)
- **Rama**: development

## Resultado (firma + body — 2026-10-14)

- **Parser** (`parseClassMember`): `partial` ante tipo de retorno de miembro marca el callable
  `partial-method`; `parseMethodTail` acepta el bodyless parcial sin `abstract`.
- **Member model**: dos `partial-method` con la misma `signatureKey` se **fusionan** (el body se
  adjunta a la firma); dos bodies definitorios → *"partial method 'M' has more than one defining
  body"*; llamada sin body → *"Stage 35 concrete OOP callable has no body"*.
- Verificado: `partial int M();` + `partial int M(){ return 5; }` → `f.M()` exit 5. Registrado
  `TL-LANG-416` (**PASS**).

## Residual (fuera del surface)

- **Emparejamiento normativo completo**: effects/defaults/attributes/generic constraints/ownership
  de las dos partes no se validan puntualmente (hoy el emparejamiento es por `signatureKey`,
  nombre+aridad+tipos). Es un refinamiento de diagnóstico, no una surface.
