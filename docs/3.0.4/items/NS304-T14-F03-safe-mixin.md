# NS304-T14-F03 — Mixin seguro

- **Estado**: COMPLETO — contextos permitidos (statement/expression/member/type) + rechazo de kind incorrecto.
- **Rama**: development

## Implementado (2026-10-15)

- `c4bbfe54` — cierre del hueco auditado:
  - **Contextos permitidos**: `MacroExpansionContext { Statement, Expression, Member, Type }` en
    `MacroExpansionEngine.h`; la definición `#macro name(params) <contexto> { body }` declara el
    contexto (por omisión `statement`), y `MacroExpansionRecord` lo publica.
  - **Rechazo de kind incorrecto**: un contexto que no sea `statement`/`expression`/`member`/`type`
    se rechaza antes de publicar el AST expandido con error causal
    `macro context must be statement, expression, member, or type`.
  - **Higiene**: ya existente (`$name` parámetro explícito + `$#name` nombre fresco
    `__nsc_macro_<scope>_<name>` sin colisión), conservada.

## Verificado O0

- `TF-FRONT-513..520` (CF-FRONT-065, Host.Frontend, `nsc_component_tests`): PASS — contexto
  expression/member/statement/type registrado; omisión ⇒ statement; contexto desconocido rechazado.
- Unit `nsc_ns3_t11_f04_hygienic_macros_tests`: PASS (9 checks, incluye los contextos).

## Contexto

- El hueco eran los contextos permitidos + la higiene + el rechazo de kind antes de publicar el AST.
  La higiene ya estaba; aquí se cerraron los contextos + el rechazo. La validación del contexto en
  el sitio de invocación (statement vs expression en el parser) pertenece al binding de T14-F05 y no
  se sustituyó por ABI inventado.
