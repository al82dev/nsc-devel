# T08-004: enlace del control C flotante

El enlazado del control C sin CRT fallo por el simbolo `_fltused`; el control
define ese marcador para ejecutar la referencia autocontenida. La matriz NSC
no se ejecuto en el primer intento y permanece pendiente.
