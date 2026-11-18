# Conteo de débiles local durante la destrucción

El control local conserva un débil implícito mientras destruye el payload.
Antes, `WeakCount` lo descontaba solo si el contador fuerte era mayor que cero.
Un destructor que consultaba reentrantemente el conteo con un débil explícito
observó dos en lugar de uno. La reproducción en un hilo devolvió 3 en O0/O2
(`A:/nsc-fixes-evidencia/local-reentrant-weak-count/before.log`).

El control local registra ahora en la palabra reservada `block[2]` si el débil
implícito está presente. `WeakCount` lo descuenta también durante el destructor;
el `finally` limpia la marca inmediatamente antes de soltar ese débil. El bloque
mantiene su tamaño anterior. En los controles y fábricas examinados, los demás
usos son los contadores en las dos primeras palabras y el slot de payload en
`block[6]`; este uso debe considerarse si evoluciona el layout del control.

La reproducción devolvió 42 en O0/O2 tras el cambio. La prueba dirigida
`nsc.sdk.shared-local-reentrant-weak-count` comprueba cero y un débil, consulta
reentrante, liberación del último débil explícito dentro del destructor y salida
excepcional del destructor. La versión anterior falla esa regresión
(`A:/nsc-fixes-evidencia/local-reentrant-baseline/result.log`). El build fue
incremental y la regresión junto con tres controles relacionados pasó 4/4
(`A:/nsc-fixes-evidencia/local-reentrant-weak-count-ctest.log`).

La prueba usa el control local interno y no verifica consumo binario. El
comportamiento aproximado de `Arc.WeakCount` durante operaciones concurrentes
no se cambió. El manejo de campos propietarios cuando el destructor propaga una
excepción permanece pendiente de la decisión solicitada.
