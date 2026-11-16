# Tanda 19: excepciones a través de un adaptador y de una especialización, y `thread_local` precompilado (T19-036)

## Alcance

Tres traslados de T19-036 que no tenían evidencia propia:

- una excepción C++ que atraviesa un adaptador de Clang (llamada virtual) y frames de NativeSharp (T12, T15);
- una excepción C++ que atraviesa un método de una especialización de plantilla de clase (T13-011);
- campos estáticos y globales de namespace `constinit thread_local` desde una plantilla precompilada (T10-004,
  T10-006).

No hizo falta cambiar el compilador: los cuatro casos pasan con el código actual. El cambio es del fixture.

## Fixture

`pruebas-tanda-19-traslados/validar.py`:

- `excepcion-adaptador`. `guarded` (C++, `try`/`catch (int)`) llama a un callback NativeSharp. El callback llama a
  `Base::fire` por un puntero a la base. En `Derived`, `fire` lanza. El IR de NSC debe llamar a un adaptador de Clang
  (`__nsc_cpp_call_…`). Si no, el caso falla. El mismo callback sobre un `Base` no lanza.
- `excepcion-especializacion`. El callback llama a `T.Box<int>.check`, un método estático inline de la
  especialización que Clang instancia y emite. Lanza con un argumento mayor que 2.
- `tls-global`. Globales de namespace `extern constinit thread_local` (un `int` y un record), leídas y escritas desde
  NativeSharp y por la biblioteca.
- `tls-campo`. El caso que ya existía (campo estático `constinit thread_local`) ahora se repite también desde una
  plantilla precompilada.

Cambios del fixture para estos casos:

- Una biblioteca de un caso con CRT se enlaza como DLL con su propio runtime estático, como en la tanda 15. Antes,
  todo caso con CRT se trataba en DLL como el rechazo de `thread_local` `dllimport`. Ahora ese rechazo se marca por
  caso (`'dll': 'tls-rechazado'`).
- La plantilla precompilada también arranca con el CRT (`--crt-startup`, `--link-lib`, lld-link).
- Hay un preludio NativeSharp y uno C++ para las funciones que llama el callback, porque los callbacks no capturan.

## Evidencia

| Caso | Estática O0–O3 | DLL O0–O3 | Precompilada, con y sin encabezado |
|---|---|---|---|
| `excepcion-adaptador` | 3102, igual que Clang; 1 adaptador | 3102, igual que Clang; 1 adaptador | no (T19-031) |
| `excepcion-especializacion` | 6001, igual que Clang | 6001, igual que Clang | no (T19-016) |
| `tls-global` | 3276, igual que Clang | no aplica: Clang rechaza `dllimport` | sí, 3276 |
| `tls-campo` | 541, igual que Clang | no aplica: Clang rechaza `dllimport` | sí, 541 |

Registros: `logs/fx-ex2a.log`, `fx-ex2e.log`, `fx-ex2ad.log`, `fx-ex2ed.log`, `fx-ex3tls.log`, `fx-ex4g.log` y
`fx-ex4gd.log`. El fixture completo, 14 casos, pasa en estática (`logs/fx-ex5.log`) y en DLL (`logs/fx-ex5d.log`).
No se cambió código de producción, así que no repetí la regresión de las tandas.

## Sigue pendiente

- La excepción por el adaptador y por la especialización desde una plantilla precompilada. Los métodos de
  instancia en una plantilla precompilada son T19-031, y los usos de plantillas C++, T19-016.
- Un `thread_local` C++ sin `constinit` ni definición visible de inicialización constante. Clang protege cada acceso
  con `__tls_guard` y `__dyn_tls_on_demand_init` del CRT. NativeSharp lo rechaza
  (`C++ thread_local needs the CRT on-demand TLS initialization`). Es el siguiente traslado de T19-036.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
