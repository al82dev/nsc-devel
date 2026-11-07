# T08-004: no descartar origenes internos antes de comprobar ambiguedad

El control de `int` e `int**` homonimos fue aceptado porque el segundo tipo
se filtraba antes de contar origenes. Se incluyen todas las declaraciones C
internas en la comprobacion de ambiguedad, sin proyectar el tipo no admitido.
Construccion incremental y validacion pendientes.
