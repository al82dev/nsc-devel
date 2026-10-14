# NS304-T09-F09 — Fallo parcial y composición

- **Estado**: IMPLEMENTADO (drop de subobjetos en fallo parcial + `throws` de constructor)
- **Rama**: development

## Estado

- **`throws E` en constructores ya existe** (F10 no es prerequisito pendiente): `public S() throws
  Exception { … throw new Exception(); }` compila y propaga la excepción nativa.
- **Fallo parcial**: los subobjetos ya inicializados (`owned Field a; owned Field b;` + `throw`) se
  dropean al fallar la construcción — el destructor `~Field()` de cada subobjeto se ejecuta
  (`Drops=2`). Una instancia parcial no se publica ni recibe el destructor de objeto completo.
- **Drop order**: el orden inverso de campos/subobjetos ya lo baja `destructionSteps`
  (`emitOneStructuredCleanup`).

## Pruebas

- `TL-LANG-472` — `S() throws Exception` con dos `owned Field` inicializados: `~Field()` ×2 (exit 7).

## Commits

- `406a198e` — fallo parcial dropea subobjetos inicializados.
