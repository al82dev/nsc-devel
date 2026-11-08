# Conciliación de typedef C hacia un enum precompilado

La comparación de enum C exigía la misma fuente física incluso cuando dos
encabezados contenían enumeraciones equivalentes. La conciliación entre módulos
admite ahora archivos distintos si el contrato completo coincide y cada fuente
disponible conserva la huella registrada. La comparación predeterminada conserva
el tratamiento previo y las definiciones distintas no se concilian. El formato
NSMETA no se modifica.

`typedefs/precompiled-enum/validar.cmd` generó una biblioteca, compiló y ejecutó
el control equivalente en O0 y rechazó tanto un valor de enumerador diferente como
una fuente modificada tras construir el productor. El control solo declara un
puntero al enum: no acredita operaciones sobre valores de enum. También pasó
`typedefs/precompiled-conflict/validar.cmd` para el record. NSC se reconstruyó
incrementalmente en A:; se observaron advertencias preexistentes por inicializadores.
La batería completa de la tanda sigue pendiente.
