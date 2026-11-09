# Referencia de patrón a un propietario move-only

## Reproducción y causa

Un `match (owner.Get())` con un enlace `ref item` rechaza una referencia de
solo lectura: el parser sintetizaba para el sujeto un local de referencia
mutable incluso cuando ninguno de los enlaces era `ref mut`. Un cambio del
parser por sí solo permitió la lectura, pero también aceptó reemplazar el
propietario mientras el enlace seguía activo. El ensayo inseguro no se conservó.

## Cambio

El sujeto de un match de sentencia se convierte en referencia de solo lectura
cuando todos sus enlaces por referencia lo son; si algún enlace es `ref mut`,
conserva la referencia mutable anterior. Para el sujeto sintético cuya llamada
devuelve una referencia de instancia ligada a `this` y cuyo receptor es un
valor move-only o una vista, el análisis registra el préstamo con la procedencia
existente. Cuando una referencia de patrón proviene de un objeto ya marcado
como préstamo de solo lectura, conserva también ese atributo y su registro.
Los orígenes que no se pueden vincular a un propietario comprobado se rechazan.

## Validación y límites

`nsc.compiler.readonly-pattern-loan` utiliza un propietario move-only mínimo,
independiente de las familias públicas en preparación. La lectura, el fin del
préstamo, el receptor ordinario y un patrón mixto ejecutaron 42 en O0 y O2;
el análisis rechazó la escritura, el reemplazo o movimiento del propietario
durante el préstamo y el origen temporal. Se ejecutó con el compilador aislado
en A. Tras el ajuste que limitó el registro a receptores move-only o vistas,
los controles existentes de patrones fundamentales, exhaustividad y guardas
volvieron a vincularse incrementalmente y pasaron.

Se probaron además casos de Rc y Arc desde fuentes, pero esas familias aún no
están registradas ni verificadas como SDK binario. Las pruebas se ejecutaron en
un árbol con otros cambios en curso: no constituyen validación aislada de la
corrección ni cubren todos los tipos de receptor, patrones condicionales, match de
expresión, formas de escape o rebindings. La salida se observó directamente y
no se conservó un log separado. La bitácora detallada de esta investigación es
`prestamo-get-punteros.md`.
