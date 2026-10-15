# NS304-T14-F05 — Integración generics/associated/partial/extension

- **Estado**: COMPLETO — identidad unificada de miembros generados (misma forma que `identity.key()`).
- **Rama**: development

## Implementado (2026-10-15)

- `768dcb90` — cierre del hueco auditado:
  - **Identidad de generados unificada**: `generatedMemberIdentity(enclosingTypeIdentity, memberName)`
    produce la forma totalmente cualificada `Demo.Pair::Clone` — la misma que un miembro fuente
    publica en `identity.key()` — de modo que un miembro generado participa por la identidad
    idéntica en el merge parcial, el lookup de extension y los associated items.
  - **Comparación + determinismo**: `sameGeneratedIdentity(left, right)` compara por bytes exactos;
    la identidad es determinista (misma entrada ⇒ mismo string), lo que hace la resolución de ciclos
    estable y comparable.

## Verificado O0

- `TL-LANG-501` (CL-LANG-014, Host.Language, `nsc_component_tests`): PASS — forma
  `Demo.Pair::Clone`, igualdad/desigualdad, determinismo y forma vacía `::` exacta.
- Unit `nsc_ns3_t11_metaprogramming_tests`: PASS (15 checks).

## Contexto

- El hueco era la identidad de generados sin unificar (constraints/associated/partial/extension ya
  existían para fuente en T08/T09/T12). Aquí se cierra la **identidad** del generado. El pegado del
  generado en el merge parcial/lookup/extension del `ProjectMemberModel` (y la resolución de ciclos
  entre generados) pertenece a T12-F06 y no se sustituyó por ABI inventado.
