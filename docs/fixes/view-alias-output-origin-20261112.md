# Origen de vistas escritas mediante alias exacto

En `8e3cf2008`, un análisis semántico aceptó que `alias = inner.Borrow()`
escribiera una vista mediante `ref View alias = ref source` y que luego
`Sink.Save(source, ref result)` transfiriera esa vista a un local que sobrevive
a `inner`. El programa negativo no se ejecutó. La reproducción y comandos están
en `A:/nsc-fixes-evidencia/view-backedge-real-20261112.py` y su `.log`.

En `ProjectMethodBodyModel::recordViewAssignmentRoot`, una asignación mediante
una referencia cuya ubicación identifica exactamente un local propagaba a ese
local identidades y observaciones desconocidas, pero no las raíces de
`viewSourceFields`. La comprobación de salida en `recordViewOutputOrigins`
consulta esas raíces. Se incorporan las raíces posibles de la asignación al
local identificado exactamente, conservando sus raíces anteriores y la
incertidumbre. El cambio no recupera identidades desde nombres ni modifica la
propagación de destinos indeterminados.

Se reconstruyó incrementalmente el target `nsc` en
`A:/nsc-fixes-evidencia/build`; Ninja detectó numerosos objetos anteriores a
sus fuentes. No se limpió el build ni se ejecutó batería completa. El nsc
resultante tuvo SHA-256
`614dd61c45b0ef90bd47b6cff4d95bd4f2339a464c58e6da670978f00077a2ca`.
La prueba dirigida `view_ref_owner_replacement.py` pasó 42 casos, incluidos el
escape directo y mediante représtamo y un control del mismo ámbito. Los dos
negativos reciben `NS3020` por el dueño interior, sin ejecutar el programa.
El control positivo compiló y ejecutó 7 en O0/O2. Registros:
`A:/nsc-fixes-evidencia/view-alias-existing-expanded-20261112.log` y
`A:/nsc-fixes-evidencia/view-alias-positive-runtime-configured-20261112.log`.
La prueba dirigida de shadowing también pasó.

Este cambio no resuelve el falso rechazo con dueños homónimos ni el flujo de
vistas entre iteraciones. La misma reproducción registra dos posibles escapes
por backedge aceptados tanto antes como después; se analizaron sin ejecución
y permanecen pendientes. Tampoco acredita destinos de alias desconocidos ni
todas las formas de vista o de llamada.
