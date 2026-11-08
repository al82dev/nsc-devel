# Tanda 8: actualizar expectativas de enum en campo C

Al integrar el carril de identidad, su tabla reflejaba el NSC anterior a las
correcciones H4 A/B de esta rama. Una ejecución agrupada de la matriz con el
compilador actual mostró que `HLevel`, `HLevelInt`, `HConst`, `HNamed`,
`HNamedType`, `CNamed` y `CInternal` compilan y ejecutan en O0. La tabla se
actualiza a `runs` solo para esos casos.

`PAnon` sigue fallando al publicar metadata con `C enum field dependency has
no importable identity`; sus dos consumidores conservan la expectativa previa.
La evidencia está en
`A:\nsc-interop-build\tanda08-identidad-integrada-20261107\enum-campo\audit-*.log`.
Esta medición es O0 y no acredita la validación completa de identidad.

Después de actualizar la tabla, el validador dirigido completo del carril
terminó con código 0 en `A:\nsc-interop-build\tanda08-identidad-integrada-20261107-2`.
Incluye expectativas de rechazo para capacidades todavía pendientes; no es una
afirmación de interoperabilidad completa ni la batería integral de la tanda.
