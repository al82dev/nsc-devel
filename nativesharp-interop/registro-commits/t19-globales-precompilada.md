# Tanda 19: globales y campos estáticos C++ en una plantilla precompilada (T19-036, T10-004, T10-006)

## Defecto

Una plantilla NativeSharp precompilada que lee o escribe una global C++ de namespace (`T.counter`, `T.origin.y`) o
un campo estático (`T.S.total`, `T.S.corner.x`) no compila en el consumidor, tenga o no el encabezado:
`unknown identifier 'T'`. La función `T.bump()` del mismo cuerpo sí se resolvía.

## Causas

1. **Productor.** `captureCppTemplateGlobalCandidates` (`CppSemanticInventory.cpp`) solo capturaba estados
   `cppCGlobal`, es decir globales C, y `captureCDependencyGlobal` (`CppCRecordMetadataCodec.cpp`) solo entradas
   `cLanguage`. Una global C++ no llegaba al catálogo.
2. **Consumidor.** `provideCppValuesWithPrecompiledCatalogs` (`CppSemanticRecord.cpp`) solo atendía demandas en el
   namespace raíz, y `projectCppPrecompiledCGlobal` proyectaba siempre con las reglas de C.
3. **Campos estáticos.** El catálogo de dependencias (`CDependencyCatalog`) tiene funciones, globales, alias,
   enumeradores, métodos estáticos y de instancia, pero ningún campo estático.

## Cambio

- **Globales C++.**
  - `CDependencyGlobalSnapshot::cppLanguage`. Una global C++ se codifica como una C seguida de un byte final; una
    global C termina en su tipo, como antes. El codec rechaza una global C++ sin enlace externo, constante,
    macro o `thread_local` bajo demanda.
  - El productor captura estados con almacenamiento (`isCppExternal`, `storageName`, no constantes) de C o de C++,
    cada uno con las entradas de su lenguaje.
  - El consumidor demanda la global en el namespace NativeSharp de su nombre calificado (`T::counter` en `T`), la
    proyecta con las reglas de su lenguaje y le da `cppCGlobal` solo si es C.
- **Campos estáticos.**
  - `CDependencyCatalog::StaticField`: el módulo y la referencia del productor; la identidad, el USR, el nombre,
    los namespaces y los records contenedores del propietario; y el campo como lo declara Clang (tipo, símbolo,
    acceso, `thread_local`, `dllimport`).
  - Van en un grupo opcional al final del catálogo de dependencias, después de los métodos de instancia, y solo
    se escriben si hay alguno. Un catálogo sin campos estáticos conserva sus bytes.
  - El productor busca demandas cuyo receptor es un record C++ con un campo estático de ese nombre (sin llamada)
    y siembra en el catálogo de tipos al propietario y el tipo record o enum del campo.
  - `provideCppStaticFieldsWithPrecompiledCatalogs`: el consumidor reconstruye al propietario con ese único campo,
    exige que su identidad y su USR coincidan con un record C++ instalado y lo proyecta con
    `projectCppSemanticStaticField`, la proyección del camino directo. `PrecompiledModuleConfiguration.cpp` lo
    instala cuando algún módulo trae campos estáticos.
  - El encabezado del consumidor importa el mismo record en su propio módulo, así que el mismo símbolo llega por
    dos propietarios. `CppImportedStaticField::sharedExternalSymbolVerified` lo permite solo si los dos
    propietarios tienen el mismo USR de Clang y los campos coinciden en nombre, forma del tipo, `const`,
    `volatile`, `dllimport` y `thread_local`, como los métodos estáticos compartidos.

## Evidencia

Casos de `pruebas-tanda-19-traslados`, comparados con Clang en O0–O3, estática y DLL. Cada uno pasa directo y desde
una plantilla precompilada, con y sin encabezado. En las declaraciones del IR, NSC tiene la forma de Clang.

| Caso | Contenido | Resultado |
|---|---|---|
| `globales-precompilada` | global escalar y global record del namespace `T`, leídas y escritas | 83318 |
| `campos-estaticos-precompilada` | campo estático escalar, record y `const` | 26796 |

Registros: `logs/fx-gl2*.log`, `logs/fx-gl4.log`. El fixture completo pasa: 11 casos en estática (`logs/fx-gl5.log`)
y 10 en DLL (`logs/fx-gl5d.log`).

Regresión dirigida (`lista-gl5.txt`: precompilado y caché de la tanda 9, tandas 10 a 18):

- 68 de 68 en estática (`logs/regresion-gl5`);
- 47 de 47 en DLL (`logs/regresion-dll-gl5d`);
- `lote-ct2`: `ninja -k 0` sin errores; las nueve CTests de interoperabilidad y `nsc_cpp_c_identity_tests` pasan;
- `ctest -R "snapshot|precompiled|record|cpp-interop|member"`: 32 de 33, falla `nsc.cpp-interop.session` sin sus
  fixtures (T19-037).

## Sigue pendiente

- Globales y campos estáticos C++ `thread_local` en una plantilla precompilada. En DLL no aplican: Clang rechaza un
  `thread_local` `dllimport`. En estática no lo he verificado.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
