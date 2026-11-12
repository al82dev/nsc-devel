# Reasignación condicional al mismo receptor

`readonly-nested-borrowed-field.md` conserva un rechazo histórico de una
reasignación condicional que vuelve a llamar a `first.Borrow()` mientras una
vista de `first` está viva. El extracto conservado contiene la reasignación
`view = false ? first.Borrow() : second.Borrow()` seguida de la extracción
prestada de `view.Get().Item`; no conserva la fuente completa de ese ensayo.

Para contrastarlo se reconstruyó un contexto con dos dueños y un campo de
clase. La fuente generada y los comandos se conservan en
`A:/nsc-vistas-evidencia/temp/view-borrow-reassignment-omgtqv_p/results.json`;
el ejecutor es
`A:/nsc-vistas-evidencia/view-borrow-reassignment-conditional-nested-probe.py`.
Se usó `vistas-env.cmd`, el nsc restaurado y Core binario de vistas.

Para Rc y Arc, ambas condiciones constantes y un control con receptores
distintos se aceptaron y ejecutaron con 42 en O0/O2. Sustituir cualquiera de
los dos dueños se rechazó con `cannot overwrite owner ... while a borrowed
local is alive`; esos programas solo se analizaron. El log es
`A:/nsc-vistas-evidencia/view-borrow-reassignment-conditional-nested.log`.
Los SHA-256 antes/después coincidieron: nsc
`7993b592c7b37a1719154ea0cccd0d1ae786cd3065ac704645b5ef027247083c`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.

El rechazo histórico no se reproduce en esta reconstrucción y configuración.
No se demuestra identidad con la fuente histórica completa ni se atribuye la
mejora a un commit concreto. Las guardas para otros receptores y procedencias
requieren controles propios. No se modificó el compilador para este contraste
ni se ejecutaron casos inseguros.
