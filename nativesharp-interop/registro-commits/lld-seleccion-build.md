# Selección de LLD en `nsc build`

El comando `build` transmite su selección validada a la invocación interna.
Cuando `NSC_LINKER` no está definido, `--linker LLD` ya no se rechaza por
falta de esa variable; el adaptador resuelve la herramienta. Cuando la variable
está definida se conserva el rechazo de una selección contradictoria.
El valor predeterminado permanece `NSLink` y no se cambia el entorno global.

La implementación debe validarse como conjunto con ejecutables, bibliotecas,
selecciones contradictorias y cachés antes de considerarla terminada. La
selección automática al detectar TLS sigue siendo una capacidad diferente y
pendiente. NSLink no se modifica.
