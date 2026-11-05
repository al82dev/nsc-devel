# Tanda 08: propietario de lambda en funcion global

La validacion del callback alcanzo HIR pero rechazo una lambda en Main porque
su ordinal representa una funcion global despues de los miembros de tipo. Se
reconoce ese espacio de ordinales con la misma convencion del modelo HIR de
funciones globales; se mantienen validaciones para ordinales fuera de rango.
Faltan pruebas agrupadas de generacion y ejecucion del callback.
