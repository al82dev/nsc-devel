# Salidas de vista a campos anidados de structs locales

En `3e6cfd8e2`, el análisis aceptó que una vista de un dueño interior se
guardara en `holder.nested.field`, directamente o mediante un alias `ref`,
aunque `holder` seguía vivo después del dueño. El control con dueño y destino
en el mismo ámbito se aceptó. Los negativos no se ejecutaron; fuente y salida
en `A:/nsc-fixes-evidencia/view-output-nested-field-before-20261112.log`.
El volcado identificó el alias como `frame-local:0` y
`L0/F:nested/F:field` (`view-output-nested-bodies-20261112.log`).

La comprobación de salida recorre ahora los campos semánticamente ligados
hasta una raíz local por valor. Exige que cada receptor sea un struct y cada
miembro sea un campo único que no sea referencia. Para un alias sigue su
cadena de représtamos y exige que el inicializador tenga esa misma raíz y que
su clave de almacenamiento precisa coincida con la del alias. Así se usa el
ámbito real del contenedor sin atribuir esa vida a clases, referencias,
índices ni proyecciones desconocidas. El recorrido es iterativo; una cadena
de représtamos se limita al número de locales.

El nsc aislado construido incrementalmente tuvo SHA-256
`73ca5ba4b760c9f59cf61869e82bb2ebabd4deb8231dc951c5f022527660175a`.
La regresión dirigida de campos pasó 14 casos, incluidos anidamientos de dos
y tres campos, alias, représtamo, dueño reemplazado y control seguro
(`view-nested-field-final-20261112.log`). También pasaron los controles
dirigidos de función global, delegado y evento, además de alias y parámetros.
Los cinco controles válidos de método, alias, función global, delegado y evento
compilaron y ejecutaron 42 en O0/O2
(`view-nested-positive-runtime-final-20261112.log`). Los negativos nunca se
ejecutaron.

Una propuesta distinta con una clase que contenía directamente una vista no
constituyó reproducción: el compilador rechaza esa declaración por su regla
de campos de vista (`view-output-class-field-probe-20261112.log`). No se
considera un defecto corregido. El presente cambio tampoco demuestra rutas
con clases, referencias como campos, índices, uniones, lugares imprecisos o
bindings desconocidos. No se ejecutó batería completa ni se modificó
interoperabilidad C/C++.
