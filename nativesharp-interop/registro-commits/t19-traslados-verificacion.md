# Tanda 19: verificación de traslados sin evidencia (T19-036, primera parte)

Fixture nuevo `pruebas-tanda-19-traslados` (todavía sin commit): compara cada caso con un control C++ de Clang
23.1.0, O0–O3, estática y DLL. Ninguno de los tres primeros casos funciona hoy. Los tres siguen pendientes.

| Caso | Traslado de origen | Resultado | Diagnóstico de NSC |
|---|---|---|---|
| Record anidado en una plantilla de clase (`Box<int>.Item`, `Box<int>.make`) | T10 → tanda 13 | no funciona | `T.Box<int>.Item it` no se analiza como tipo (`expected ';'`); con `var`, `has no compatible member named 'make'` y `C/C++ parameter 0 is not yet mapped` en una función con parámetro `Box<int>::Item` |
| `vectorcall` con agregado homogéneo de dos `double` por valor y devuelto | T09-003 (matriz de firmas) | no funciona | `C/C++ return type is not yet mapped`, `C/C++ parameter 0 is not yet mapped` |
| Campo estático `thread_local` (constante), estática con `--crt-startup` | T10-006 → T16-006 | no funciona | `has no compatible member named 'counter'`: `projectCppSemanticStaticField` (`CppSemanticRecord.cpp`) descarta todo campo estático `thread_local` |
| Campo estático `thread_local` con DLL | — | no aplica | Clang rechaza un `thread_local` `dllimport`: el ABI de Microsoft no lo permite |

Evidencia:

- `A:/temp/nsc-t19-traslados-7bq87dyt`: anidado con tipo explícito;
- `A:/temp/nsc-t19-traslados-lfm53tf3`: anidado con `var`;
- `A:/temp/nsc-t19-traslados-t50xxt91`: `vectorcall`;
- `A:/temp/nsc-t19-traslados-1wokk1mj` y `-nl27smlj`: `thread_local`, estática y DLL.

Siguientes casos de T19-036, sin ejecutar todavía:

- `class` en el catálogo precompilado;
- globales y campos estáticos en una plantilla precompilada;
- excepciones a través de un adaptador y de una especialización.

El fixture entra al repositorio cuando sus casos pasen; mientras tanto no se agrega a la batería.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
