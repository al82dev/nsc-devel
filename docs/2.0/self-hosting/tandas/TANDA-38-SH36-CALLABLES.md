# NativeSharp 2.0 self-hosting — tanda 38: grafo de callables

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH36` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `22c8afd87` |
| Commit de cierre | este commit (`selfhost(tanda-38): publish the callable graph`) |

## Implementación y gate

`CallableGraph.Build` recorre el árbol completo y registra delegates, lambdas, calls, métodos
virtuales y overrides. Deduplica y ordena ordinalmente, por lo que el resultado no depende de rutas,
perfil ni enumeración del filesystem. `--dump-project-callables` lo publica como contrato estable.

La fixture contiene un delegate, lambda, par virtual/override y una llamada directa ejecutable. El
grafo esperado tiene cinco entradas idénticas en O0–O3; el objeto enlaza con LLD y NSLink y devuelve
42 en las ocho rutas. El layout físico completo de vtables/callbacks de ABI se vuelve a certificar
en SH46; SH36 fija aquí la identidad semántica que esas tablas consumen.

- [x] grafo canónico de delegates, lambdas y despacho.
- [x] identidad independiente de perfil y ruta.
- [x] ejecución O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
