# NS3-T16-F02 — Lints de migración / Migration lints

## Español

`nsc --analyze-migration <archivo.ns>` analiza fuente 2.x sin compilarla ni modificarla. El reporte
estable `NSMIGRATION-ANALYSIS 1` ubica riesgos de palabras contextuales nuevas, selección de overload
con `null`, copy→move, nullability de punteros, imports `using`, conversiones numéricas y orden de
cleanup. Cada hallazgo conserva el código `MIG3001`…`MIG3007`, línea/columna y una alternativa.

El scanner ignora comentarios y literales. `// nsc-migration:generated` excluye archivos generados;
`// nsc-migration:disable-next-line MIG3006` suprime sólo el código indicado en la línea siguiente.
Los lints no aplican cambios: un posible falso positivo queda visible para revisión.

## English

`nsc --analyze-migration <file.ns>` inspects 2.x source without compiling or changing it. The stable
`NSMIGRATION-ANALYSIS 1` report locates newly contextual words, null-driven overload selection,
copy-to-move changes, pointer nullability, `using` imports, numeric conversions, and cleanup order.
Every finding carries its `MIG3001`…`MIG3007` code, line/column, and an actionable alternative.

Comments and literals are ignored. `// nsc-migration:generated` excludes generated files and
`// nsc-migration:disable-next-line MIG3006` suppresses only the requested code on the next line.
Lints never edit source, so uncertain cases remain explicit review items.

## Validación dirigida / Targeted validation

Se construye y ejecuta únicamente `nsc_ns3_t16_migration_tests`; no se ejecuta CTest ni baseline.
