# NS304-T12-F04 — Constructores parciales

- **Estado**: IMPLEMENTADO (firma + body definitorio fusionados; initializer/base/this único = residual)
- **Rama**: development

## Resultado (firma + body — 2026-10-14)

- **Parser** (`parseConstructorTail`): `partial` marca el constructor `partial-method` y acepta el
  bodyless (`Foo();`) sin exigir body.
- **Member model**: dos `partial-method` constructores con la misma `signatureKey` se fusionan (body
  adjunto a la firma), reutilizando F02.
- Verificado: `partial Foo();` + `partial Foo(){ X = 5; }` → `f.X` exit 5. Registrado `TL-LANG-418`
  (**PASS**).

## Residual (fuera del surface)

- **Un solo body con initializer/base/this** + la unificación normativa de firma/effects/required/init
  (hoy el emparejamiento es por `signatureKey`; no se valida el resto del contrato ni se prohíbe un
  initializer en ambas partes).
