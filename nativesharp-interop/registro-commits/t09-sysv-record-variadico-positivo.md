# T09-003: el record variádico SysV de 20 bytes pasa a comparación positiva

## Motivo

`pruebas-tanda-09-sysv/validar.py` usaba `bad.h` como control negativo. La firma
`__attribute__((sysv_abi)) T09Unsupported sysv_record(float, T09Other, float, ...)`
(record de 20 bytes devuelto, `T09Other` de 12 bytes por valor) debía rechazarse con
`sysv_abi currently supports only selected`. Un avance anterior de T09-003 la admitió
con plan Clang validado, y la validación agrupada del 2026-11-14 falló porque NSC ya
no la rechazaba (`A:/temp/nsc-t09-sysv-prueba-4ioyds3h`). Por indicación del usuario,
el caso se convierte en comparación positiva contra Clang. Además se añade un
control negativo separado para una firma que NSC sigue sin admitir, con la misma
comprobación de rechazo que antes.

## Cambios en la prueba

- `record-variadico.c`: implementación C de la declaración de `bad.h`, que no se
  modifica. Cada campo del record devuelto depende de un argumento fijo, incluidos las
  dos mitades de `T09Other.x` y `T09Other.y`. La parte variádica no se lee porque el
  destino Windows de Clang no ofrece `va_list` SysV.
- `ControlRecordVariadico.c` y `RecordVariadico.ns`: llaman la función dos veces, sin
  argumento variádico y con un `int` variádico. Comprueban los cinco campos
  (7, 7, 1000, 3, 350) con un código de salida distinto por campo.
- `validar.py`, en estática y DLL de O0 a O3:
  - compila el productor y el control con Clang y ejecuta el control;
  - en O0 exige que las dos llamadas de NSC tengan el mismo tipo físico que las de
    Clang: `void (ptr, float, i64, i32, float, ...)` con `sret`;
  - construye y ejecuta el consumidor NativeSharp.
- Control negativo nuevo (`hook.h`, `Hook.ns`, `Hook.nsproj`): un record C con un campo
  puntero a función, `T09Hook sysv_hook(T09Hook)`. Antes de comprobar que NSC lo
  rechaza con el mismo texto `sysv_abi currently supports only selected`, el script
  exige que Clang acepte la declaración.
  - Se eligió después de una sonda de candidatos
    (`build-claude-t08/tools/sonda-sysv-negativa-v3.py`). NSC admite hoy los records
    empaquetados, las uniones, los campos de bits, cinco records mixtos, un record de
    40 bytes y la variádica con dos records. Rechaza este record y el retorno de puntero
    a función. Evidencia: `A:/temp/nsc-t09-sysv-sonda-negativa-mbd_xnjm`,
    `A:/temp/nsc-t09-sysv-sonda-negativa-ms74ukbw`.
- `bad.h` se conserva y lo usa el caso positivo. `Bad.ns` y `Bad.nsproj` quedan sin
  uso en el script. No se borraron.
- El control existente de conversión `sysv_abi` a callback no cambia.

## Validación

- `pruebas-tanda-09-sysv/validar.py`: código 0. Pasan las ocho comparaciones
  previas, la plantilla precompilada, las ocho comparaciones del record variádico, el
  rechazo de `sysv_hook` y el rechazo del callback
  (`A:/temp/nsc-t09-sysv-prueba-bppwclz_`).
- Sensibilidad: con una biblioteca alterada (`extra` + 1), el consumidor NativeSharp
  termina con 5 (`build-claude-t08/tools/mutacion-record-v2.py`).
- Se repitió una vez la validación agrupada de la tanda: 24 de 24 validadores pasan
  (ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md)).

## Límites

No se verifica la lectura de argumentos variádicos SysV en el receptor. No se cubren
otros tamaños ni formas de record variádico. T09-003 y la tanda 9 siguen abiertos.
