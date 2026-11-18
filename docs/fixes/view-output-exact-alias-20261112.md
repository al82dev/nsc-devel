# Destino real de una salida de vista a través de ref

En `ce46c33b1`, el análisis aceptó transferir por `ref/out` una vista de un
dueño interior hacia una referencia local que apuntaba a una vista exterior;
también aceptó un représtamo. Los tres programas negativos se analizaron sin
ejecutarlos. Reproducción y control seguro:
`A:/nsc-fixes-evidencia/view-output-ref-alias-before-20261112.log`.
Además, se reprodujo un escape mediante un alias de un parámetro `ref` del
llamador; el volcado registró `escape-origin=external:0` y `borrow-place=P0`
(`view-output-alias-parameter-valid-20261112.log` y
`view-output-alias-parameter-bodies-20261112.log`).

`recordViewOutputOrigins` comparaba la vida del dueño con el ámbito del alias,
no con el del almacenamiento al que apuntaba. Cuando el origen y la clave de
lugar identifican exactamente un local del mismo callable, usa ahora ese
local como destino y conserva las raíces tanto en el local como en su alias.
Cuando identifican exactamente un parámetro `ref/out` mediante `P<ordinal>`,
se aplica el control ya existente para salidas al llamador. Las demás
procedencias conservan sus reglas; no se infiere identidad desde un nombre.

Tras compilar incrementalmente el target `nsc`, el compilador aislado tuvo
SHA-256 `c94f74f0e693ca35100ffcfd39363fad6b403822f34889c8654eb50165748961`.
La regresión `view_ref_owner_replacement.py` rechazó los escapes por alias y
représtamo, preservó el control del mismo ámbito y rechazó la sustitución del
dueño con la vista todavía activa. Las variantes con funciones globales,
delegados y eventos pasaron sus controles dirigidos. Registros con prefijos
`view-output-alias-` en `A:/nsc-fixes-evidencia`.

`view_output_parameter_alias.py` pasó siete comprobaciones: cuatro rechazos
de escapes mediante alias, incluidas salidas `ref/out`, y tres controles
seguros (`view-output-parameter-alias-expanded-20261112.log`). Los dos controles
seguros ejecutados por separado, con destino local y parámetro, devolvieron 42
en O0/O2 (`view-output-alias-positive-20261112.log` y
`view-output-parameter-positive-20261112.log`). También pasaron las pruebas
dirigidas de shadowing y tuplas.

La corrección no certifica alias con ubicación desconocida, proyecciones,
almacenamiento externo sin identidad exacta ni todos los productores de vista.
No se ejecutó la batería completa ni se modificó interoperabilidad C/C++.
