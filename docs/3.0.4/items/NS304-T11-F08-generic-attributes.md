# NS304-T11-F08 — Atributos genéricos

- **Estado**: IMPLEMENTADO (identidad genérica `Attr<T,...>` + emparejamiento por nombre base; consteval/NSMETA residuales)
- **Rama**: development

## Resultado (identidad genérica + emparejamiento — 2026-10-14)

- **ComptimeEngine** (`attributeIdentity`): admite `<`, `>`, `,`, `::` en la identidad de un
  atributo, de modo que `attribute Tag<T>` y `Tag<int>` son identidades válidas.
- **ProjectMetaprogrammingModel** (`attributeBaseName`): la definición y el uso se emparejan por el
  **nombre base** (el tramo antes del `<`), de modo que `Tag<int>` resuelve la definición `Tag<T>`.
- Prueba: `TL-LANG-476` — `#[attribute Tag<T> targets(type)]` + `#[Tag<int>] class Program` → exit 7
  (**PASS**).

## Residual (fuera del surface)

- **Validación consteval**: los argumentos genéricos `<...>` no se resuelven ni validan como tipos
  consteval; un argumento runtime o ref-like no representable aún no se rechaza puntualmente.
- **Serialización NSMETA**: la identidad genérica viaja como cadena cerrada, sin instanciación
  semántica ni verificación de duplicados incompatibles por especialización.

## Auditoría (línea base)

Contrato (tracking): *Implementar atributos genéricos*.
Plan T11-F08 (DeepSeekDocs): *aceptar tipo de atributo genérico y argumentos consteval válidos; cerrar
instanciación, identidad y serialización NSMETA*.

## Commits

- `c70af4c0` — atributos genéricos por identidad de nombre base (TL-LANG-476).
