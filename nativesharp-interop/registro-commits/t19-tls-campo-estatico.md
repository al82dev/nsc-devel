# Tanda 19: campos estáticos C++ `thread_local` (T19-036, T10-006)

## Defecto

La [verificación de traslados](t19-traslados-verificacion.md) mostró que un campo estático C++ `thread_local`
no se importa. El cierre de la tanda 10 lo había trasladado a T16-006, que solo comprobó el TLS dentro de la
biblioteca. NativeSharp decía `has no compatible member named 'counter'`.

## Causa

`projectCppSemanticStaticField` (`CppSemanticRecord.cpp`) descartaba todo campo estático con `threadLocal`. El
camino de las globales C++ `thread_local` (T10-004) ya proyectaba como TLS de Windows las de inicialización
constante:

- `NamespaceStateSymbol::cppThreadLocal` llega a `ProjectOopMirStaticField::cppThreadLocal`;
- `OopLlvmStaticState.cpp` marca la global externa como `thread_local`.

Los campos estáticos no tenían ese dato en ningún punto.

## Cambio

- `ClangSession.cpp`: para un campo estático `thread_local`, la misma regla bajo demanda de una global
  (T10-004). Es `CppStaticField::cppTlsOnDemand` cuando no hay `constinit` ni una definición visible de
  inicialización constante.
- `CppSemanticRecord.cpp`: el campo se proyecta salvo que sea bajo demanda o `dllimport`. Su `threadLocal`
  llega a `CppSemanticStaticField` y a `CppImportedStaticField`. La comparación de records también compara
  `cppTlsOnDemand`.
- `ProjectMemberModel.cpp` lo copia a `ClassMemberSymbol::cppThreadLocal`, y `MirBuildCoordinator.cpp` a
  `ProjectOopMirStaticField::cppThreadLocal`. El backend no cambia: ya emite el acceso TLS.
- Se agregó el campo a `CppRecordDeclaration.h`, `CppSemanticRecord.h`, `CppImportedStaticField.h` y
  `ClassMemberSymbol.h`.

Siguen pendientes, sin cambios:

- un campo `thread_local` bajo demanda: lo mismo que las globales, requiere la inicialización bajo demanda del
  CRT desde NativeSharp (T19-034);
- en una DLL: Clang rechaza un `thread_local` `dllimport`, porque el ABI de Microsoft no lo permite; no aplica.

## Evidencia

El fixture nuevo `pruebas-tanda-19-traslados`, caso `tls-campo`, usa `static constinit thread_local int
counter`. NativeSharp lo lee, lo escribe y llama a una función de la biblioteca que lo incrementa.

- Estática con `--crt-startup`: 541, igual que Clang, O0–O3 (`logs/fx-tl2.log`).
- `--dll`: Clang rechaza el `thread_local` `dllimport`; no aplica.

Los casos que todavía no funcionan están en `casos_pendientes.py` del mismo fixture: record anidado en una
plantilla y `vectorcall` con agregado homogéneo.

Regresión dirigida (`lista-tl1.txt`, tandas 10, 11, 12, 13 y 16):

- 46 de 46 en estática (`logs/regresion-tl1`);
- 29 de 29 en DLL (`logs/regresion-dll-tl1d`);
- `lote-ct2`: `ninja -k 0` sin errores; las CTests de interoperabilidad y `nsc_cpp_c_identity_tests` pasan.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
