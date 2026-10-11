# Validacion 207 - Stores de simbolos promovidos

## Alcance

`TF-SSA-134` (`CF-SSA-017`) y `TF-SSA-198` (`CF-SSA-025`).

## Causa confirmada

La variante `5` agrega un local cuya direccion es tomada. Ese simbolo no es
promovible y conserva correctamente sus operaciones de almacenamiento. Los
oraculos contaban todos los `StoreSymbol` del cuerpo y atribuian ese storage
legitimo al conjunto promovido.

## Correccion

Un contador local cruza cada `StoreSymbol` con `SsaFunction::isPromoted`. Los
dos contratos exigen ahora ausencia exclusivamente para stores de simbolos
promovidos y permiten que el storage no promovible siga observable.

No se modifico el constructor SSA ni el verificador del producto.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
