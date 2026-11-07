# Resolución explícita del enlazador en NSC

Se añade al adaptador de enlazado de NSC una entrada explícita de resolución
para `NSLink` o `LLD`, sin cambiar el descubrimiento histórico usado por los
llamadores existentes. Cuando hay `NSC_LINKER`, se comprueba que corresponda
a la selección; sin esa variable, LLD se busca al lado de NSC o en el PATH.
No se modifica el código fuente de NSLink ni el entorno del proceso.

Esta capacidad es infraestructura pendiente de conectar al comando y a las
identidades de caché. Todavía no prueba que `nsc build --linker LLD` funcione
sin la variable ni cambia su comportamiento. Las pruebas se ejecutarán en
grupo después de completar la propagación.
