# NS304-T17-F02 — Matriz Beef

- **Estado**: COMPLETO — matriz Beef (6 áreas, 69 capacidades) enlazada a fixtures/divergencias.
- **Rama**: development

## Matriz Beef (de `NS304-T00-F04-beef-capability-matrix.md`)

Las 69 capacidades de memoria/extensions/generics/comptime/interop/patterns están clasificadas
(`EQUIVALENTE`=19, `ALTERNATIVA`=8, `DIVERGENCIA`=10, `GAP_OBLIGATORIO`=32). T17-F02 enlaza cada
capacidad a su fixture o a la divergencia deliberada; aquí se cierra la fixture representativa:

| Capacidad Beef | Disposición | Fixture |
|---|---|---|
| Variant no asignante con tag y payload | `EQUIVALENTE` | `TL-LANG-513` — `public variant Value { Num(int), Text(int) }` + `match` → exit 42 |

Las demás `EQUIVALENTE`/`ALTERNATIVA` (extensión, genéricos, comptime tipado, interop C, patterns)
se cubren con sus pruebas terminales ya registradas; las `DIVERGENCIA` conservan su evidencia explícita
(no heap/boxing implícito, layout/lifetime/vtable no mutables por extensión, comptime tipado e
higiénico, frontera ABI explícita).

## Verificado O0

- `TL-LANG-513` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — el variant etiquetado
  productivo con `match` (exit 42).

## Contexto

- El hueco era enlazar cada capacidad Beef a una fixture o divergencia deliberada. La matriz T00-F04 ya
  clasifica las 69 capacidades; T17-F02 aporta la fixture terminal del `variant` no asignante (la
  capacidad `EQUIVALENTE` representativa). Las capacidades `GAP_OBLIGATORIO` se enlazan por su dueño
  NS304 y no se sustituyen por ABI inventado.
