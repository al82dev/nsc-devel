# T08-D20: gate histórico con CRLF, caché vigente y relleno TLS

Fecha: 2026-11-09. Casillas: T08-D20 y T08-GATE-03a ([estado anterior](t08-validacion-integral-20261109.md)).

## Problema

El gate histórico `validar-tanda-08.cmd` terminaba con código 1 por cuatro validadores, ninguno por un fallo del
compilador:

| Validador | Causa |
|---|---|
| `identidad-c\validar.cmd` | Finales LF: cmd no encontraba la etiqueta `setup_atomic` |
| `record-c\precompiled-bool\validar-cache.cmd` | Esperaba el diagnóstico «C/C++ project references require --no-artifact-cache until transitive headers are tracked», retirado por `03bf12e42` |
| `tls-c\atomic\validar.cmd` | La misma expectativa retirada |
| `tls-c\directo\validar-relleno-cero.cmd` | El control C con la DLL de relleno TLS devolvía 1 a veces; se había clasificado como una carrera |

## Causa raíz

- **LF:** resuelto por T08-D08 (`.gitattributes` con `eol=crlf` para `.cmd`).
- **Caché:** desde `03bf12e42`, un proyecto con referencias C/C++ compila con `--artifact-cache`. Los dos
  validadores seguían esperando el rechazo.
- **`tls-c\atomic`:** al llegar más lejos apareció una tercera expectativa obsoleta. `Unsupported.ns` usa
  `atomic_unsupported_record` (un `_Atomic` record thread-local) y esperaba el rechazo. T08-D07 lo soporta.
- **Relleno TLS:** no era una carrera. `prepare_zero_fill.py` reduce el patrón TLS de la DLL en 5 bytes y declara
  `SizeOfZeroFill = 5`. Clang coloca `tls_zero` y `tls_bool` al final del patrón, así que los dos quedan en esos
  5 bytes. **El cargador de Windows no pone en cero esos bytes**
  del bloque TLS de cada hilo: los deja con lo que traía el heap.

  La sonda `A:\nsc-interop-build\claude\sondas\relleno\dump.c` imprime el bloque TLS de la DLL en el hilo
  principal:
  - en una ejecución, los bytes 64–71 valen cero;
  - en las siguientes, contienen un puntero del heap (`b0b5480080000000`), y `tls_zero_read()` devuelve ese valor.

  El control C esperaba `tls_zero == 0`, y el programa NativeSharp también (`tls_zero_read() != 0` → 8). Los dos
  dependían del contenido del heap. Con el mismo binario, el control C devolvió 1 cinco veces seguidas.

## Cambio

Lo autoriza el punto 20 del usuario.

- **`validar-cache.cmd`:** el productor con `--artifact-cache` debe compilar. El consumidor mantiene su
  comprobación de acierto de caché.
- **`tls-c\atomic\validar.cmd`:**
  - la compilación con caché debe ejecutar, y una segunda compilación debe acertar en la caché;
  - el registro `_Atomic` thread-local debe compilar (T08-D07);
  - el puntero calificado `_Atomic(long double*)` sigue rechazado.
- **`validar-relleno-cero.cmd`:** ya no se espera un valor inicial en los bytes de relleno, porque Windows no lo
  garantiza. Los controles nuevos son deterministas:
  - `RellenoControl.c` (C): la DLL carga y su propia comprobación (`tls_scalar_check`) ve los valores del patrón,
    no los que escribe NativeSharp;
  - `RellenoNative.ns` con `RellenoProject.nsproj` (NativeSharp, O0–O3):
    - lee los valores del patrón, pero no `tls_zero` ni `tls_bool`, que están en el relleno;
    - escribe `tls_zero` y comprueba que la DLL lee ese valor;
    - escribe el resto y comprueba que `tls_scalar_check()` da 0.

  `ScalarControl.c` y `ScalarNative.ns` siguen igual: los usa `validar-escalar-crt.cmd` con la DLL sin relleno,
  donde `tls_zero` está en el patrón y sí vale cero.
- **`validar-tanda-08-derivados.py`:** ya no aplica cambios, porque los validadores versionados los llevan. Sigue
  ejecutándolos desde una copia CRLF.

## Validación

`A:\nsc-interop-build\claude\scripts\regresion-d20.cmd`, resultado 0:

- `identidad-c\validar.cmd`, `validar-cache.cmd` y `tls-c\atomic\validar.cmd`, sin copias ni derivados;
- `validar-relleno-cero.cmd` cinco veces seguidas, y cada ejecutable NativeSharp O0–O3 25 veces más.
  La primera versión leía `tls_bool` antes de escribirlo y devolvía 6 en 12 de 20 ejecuciones: la misma causa;
- `validar-tanda-08-derivados.py`.

El gate histórico completo se ejecuta en la validación integral del cierre de la tanda.
