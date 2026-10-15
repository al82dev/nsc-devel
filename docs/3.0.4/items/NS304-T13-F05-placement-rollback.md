# NS304-T13-F05 — Placement/in-place y rollback

- **Estado**: IMPLEMENTADO (rollback de fallo parcial verificado en object/placement/raw)
- **Rama**: development

## Resultado (2026-10-14)

- El drop de subobjetos en fallo parcial de constructor funciona **para structs**, tanto en
  **object construction** (`S s = new S(1)`) como en **placement** (`new (alloc) S(1)`) y
  **placement crudo** (`new (storage) S(1)`), sin doble-drop ni crash.
- Verificado O0: `Field.Drops == 1` por cada escenario de fallo parcial (solo `a` construido, `throw`,
  `b` nunca construido); `Field.Drops == 2` en fallo total (ambos construidos, luego `throw`, ya cubierto
  por `TL-LANG-472`).
- El reporte previo (`A:\HelpDeepSeek\NS304-T13-F05.md`) documentaba `Field.Drops == 0`; ese hueco ya
  quedó cerrado por el trabajo de drop inverso (T09) previo a esta pasada.

## Nota (orden de drop)

El drop de los campos `owned` en el unwind del constructor se emite en **orden de declaración**
(forward, `a` luego `b`), no en orden inverso. Para campos `owned` independientes no tiene impacto
observable de corrección (no hay interdependencia); se registra aquí por transparencia.

## Prueba

- `TL-LANG-491` — fallo parcial en object + allocator placement + raw placement → `Field.Drops == 3`
  (un drop por escenario), exit 0. Guard `TL-LANG-472` PASS.
