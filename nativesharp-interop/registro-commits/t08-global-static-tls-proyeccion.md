# T08-004: proyeccion de global C estatica TLS int

El control C paso y NSC rechazo `local_value` como identificador desconocido.
Se admite la forma reproducida `static _Thread_local int` sin calificadores,
conservando la propiedad TLS de la declaracion. Construccion y pruebas
permanecen pendientes.
