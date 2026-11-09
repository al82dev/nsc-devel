# T08: validación integral después de T08-D13 y T08-015-11 (2026-11-09, noche)

Compilador: `A:\nsc-interop-build\tanda1-ON`, construido con los commits hasta `02bcd5dfc` (T08-015-11).

Script: `A:\nsc-interop-build\claude\scripts\integral-cierre.cmd`.

## Resultado

- **Gate histórico `validar-tanda-08.cmd`: código 0.**
- **Gate complementario `validar-tanda-08-complementaria.cmd`: código 0.** Ahora ejecuta dos suites más:
  - `volatile-alias-c\validar-alias-modo.py` (T08-D13 y T08-015-11);
  - `enteros-c\sonda-semantica-c.py` (instantánea de T08-D11 y T08-D12).

Duración total: 23 minutos y medio. Los registros están en `claude\logs\integral-historico.log` e
`integral-complementario.log`.

## Fuera de los gates (ctest)

**Pasan:**

- `nsc.cpp-interop.session`, `nsc.compilation-model`, `nsc.project-loader`, `nsc.project-graph`;
- `nsc.namespace-function-metadata`, `nsc.item343` a `nsc.item345`;
- `nsc.stage48-2.*`, `nsc.stage48_1`, `nsc.stage48_3`, `nsc.stage48_4`, `nsc.stage50_9`;
- `nsc.library-metadata-version-tolerance` y `nsc.library-metadata-merger`.

**Fallos conocidos y ajenos a esta tanda:**

- `nsc.stage48_2.library-metadata`: 442 de 444. Falta el fixture 1.46; ver
  [la investigación](t08-d24-fixture-historico-146.md).
- `nsc.project-oop-lowering`: 25 de 660. Falla igual sin los cambios de D13
  ([detalle](t08-d13-alias-volatile-modo-origen.md)).
- `nsc.item340` a `nsc.item342` y `nsc.item346` a `nsc.item349`: sus ejecutables no están construidos en
  `tanda1-ON`. No forman parte de las pruebas de la tanda.

## T08-D21: reagrupación de commits

**El historial está publicado.** `github/development-cpp-clang` está en `ccbcd2f66`. Ahí hay 1881 commits con
`t08` en el asunto, entre el 2026-09-29 y el 2026-11-09; casi todos son del 5 al 9 de noviembre, con nombres
`interop-t08-*` de un solo detalle. Solo cuatro commits locales no están publicados: los de T08-D11/D12/D04b/D05b/D24,
T08-D13, T08-015-11 y el de esta validación.

**Por qué no se reagrupó.**

1. La instrucción era reagrupar cuando la tanda 8 estuviera terminada y validada. La tanda valida, pero no está
   terminada: quedan las decisiones de D04b, D05b, D11, D12, el fixture de D24 y el pendiente funcional T08-D13b.
2. Reagrupar commits publicados reescribe un historial publicado, y para publicarlo haría falta un push forzado.
   La regla vigente es no hacer push, y además esto exige una autorización explícita.

Los commits locales ya están agrupados por cambio funcional: documentación de las decisiones, contrato del alias,
condicional flexible y validación. No hace falta reagruparlos.

**Respaldo.** Nuevo bundle `claude\respaldo\development-cpp-clang-20261109-noche.bundle`, verificado
(`git bundle verify`), con la rama completa hasta `02bcd5dfc`. El bundle anterior se conserva.

**Propuesta para cuando se decida.** Agrupar los commits publicados por bloque funcional del tracking, sobre una
rama nueva y sin tocar la publicada:

- T08-002 a T08-020, uno o pocos commits por bloque;
- decisiones T08-D01 a T08-D24;
- validaciones T08-GATE.

Así se obtendrían del orden de 40 a 60 commits en lugar de unos 1800. Reemplazar `development-cpp-clang` o publicar
la rama nueva lo decide el usuario.
