# T08-010/015: contrato C del alias inferido

El análisis conserva un marcador de contrato C en un local `var` inferido de
un miembro flexible C volatile, y lo transporta a las expresiones que nombran
ese local y sus bindings. Un alias inferido de otro alias marcado también lo
conserva. El marcador describe el contrato estático del alias, no el origen
dinámico de la dirección ni una elección de orden de memoria.

Todavía no modifica HIR ni los accesos. Parámetros, conversiones explícitas,
expresiones compuestas, flujos entre funciones y persistencia no quedan cubiertos
por este cambio. La protección de configuración explícita sigue activa; no se
ha construido ni validado. Estas rutas deben resolverse antes de activar el modo.
