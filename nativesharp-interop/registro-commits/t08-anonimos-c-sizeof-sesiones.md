# Layout de valor para el propietario C observado en dos sesiones

`Repeat.nsproj` ya proyectaba los miembros promovidos, pero `sizeof(LAnonymous)`
fallaba porque el propietario con dos observaciones solo tenía layout de acceso.
Para un record C con carriers anónimos, se habilita el layout de valor únicamente
cuando el grupo ya fue verificado y las definiciones completas comparten USR,
archivo equivalente, hash y offset de origen, dimensiones y campos capturados.
Los demás records repetidos conservan la regla anterior.

El cambio no se ha construido ni probado todavía. La tanda 8 sigue abierta.
