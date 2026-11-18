# Destrucción explícita de campos propietarios

El contrato `NSC-PLACEMENT-003` rechaza `destroy` de valores owned. La lectura
de un campo puede producir una expresión no propietaria aunque el campo siga
siendo dueño del objeto. En `normal_factory_base_owned_destroy` y su control
sin excepción, el compilador aceptaba `destroy resource` sobre un campo owned;
las ejecuciones iniciales en Rc O0 observaron más de una destrucción. Las
reproducciones se conservan en `tests/integration/shared_public_pointers.py`.

El análisis consulta la declaración identificada por el ordinal de campo,
además de la propiedad de la expresión, y rechaza `destroy` si el campo es
owned. No modifica el contrato de `delete` ni cambia la limpieza automática.
La compilación incremental del compilador y la prueba dirigida usó
`A:/nsc-fixes-evidencia/build-owned-destroy-check.cmd`; el registro está junto
al script con extensión `.log`.

`owned-destroy-check.cmd` pasó la prueba existente de destrucción explícita:
23 comprobaciones, incluidos el receptor implícito y el campo de otro objeto,
así como los controles previos de colocación y operandos inválidos. El runner
de `owned-destroy-public-check.cmd` confirmó el rechazo semántico de los dos
ejemplos originales con las fuentes de Rc y Arc. Es una comprobación estática,
sin ejecución ni distinción de optimización, y no prueba escapes a través de
alias que pierden la identidad del campo ni cubre todos los módulos compilados.

Una repetición posterior añadió campos anidados y acceso entre paréntesis a la
misma prueba; las 27 comprobaciones pasaron (`owned-destroy-expanded.log`).
La repetición con la compilación incremental posterior volvió a pasar las
27 comprobaciones (`owned-destroy-current.log`).
El control sigue sin demostrar que un alias local conserve la identidad del
campo propietario.
