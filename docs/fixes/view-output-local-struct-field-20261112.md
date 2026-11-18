# Salida de vista a campo directo de struct local

En `ffe7959e7`, `view-output-field-probe-20261112.py` mostró que escribir por
`ref` una vista del dueño interior en un campo de un struct local exterior,
directamente o mediante un alias, se aceptaba sin diagnóstico. El control con
dueño y destino en el mismo ámbito se aceptó. Los negativos solo se analizaron.
El volcado del alias identifica el almacenamiento como `frame-local:0` y
`L0/F:field` (`view-output-field-bodies-20261112.log`).

`recordViewOutputOrigins` identifica ahora campos directos cuyo receptor es
un struct local por valor y aliases con una clave exacta de un único campo de
ese struct. Verifica la vida contra el local contenedor y conserva en él las
raíces posibles y sus identidades, sin convertir Unknown en evidencia exacta.
La primera prueba de identidad mostró que rechazar solo el escape al salir
del ámbito no impedía reemplazar el dueño después; se incorporó la propagación
de identidades y ese control pasó. Los campos de clases, punteros, índices y
proyecciones más profundas no se asimilan a un struct local por esta regla.

Las llamadas de delegado y evento preparaban inicialmente la proyección sin
recuperar el tipo del campo y seguían aceptando el escape. La preparación de
argumentos usa ahora el tipo semántico registrado del campo; el control de
backedges hace lo mismo para estas salidas. Los logs
`view-field-delegate-20261112.log` y `view-field-event-20261112.log` conservan
ese fallo anterior a la corrección.

El nsc aislado construido incrementalmente tuvo SHA-256
`d3a5be406a3d0f12a6bdd5051242f814431394d310978aea6d4b30f4753d1c8c`.
`view_output_struct_field.py` pasó ocho comprobaciones semánticas de ref/out,
alias, représtamo, ámbito seguro y reemplazo del dueño. Los controles de
función global, delegado y evento rechazaron los escapes directos; delegado y
evento también rechazaron el escape entre iteraciones y el reemplazo del dueño.
Los negativos no se ejecutaron. Las pruebas positivas de cinco rutas
compilaron y ejecutaron 42 en O0/O2. Registros y comandos en
`A:/nsc-fixes-evidencia/view-output-field-final-20261112.log`,
`view-field-namespace-expanded-20261112.log`,
`view-field-delegate-expanded-20261112.log`,
`view-field-event-expanded-corrected-20261112.log` y
`view-field-positive-runtime-20261112.log`. También pasaron las regresiones
dirigidas de alias, parámetros, shadowing y tuplas.

La corrección se limita a la proyección demostrada: no acredita clases,
almacenamiento externo, índices, campos anidados, alias inciertos ni todos los
productores o ciclos. No se ejecutó batería completa ni se modificó
interoperabilidad C/C++.

La ampliación posterior a campos anidados de structs por valor está registrada
en `view-output-nested-struct-fields-20261112.md`; esta sección conserva los
límites y resultados de la revisión anterior.
