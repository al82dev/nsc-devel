# Tanda 19: `thread_local` C++ con inicialización dinámica (T19-036, T10-004, T10-006)

## Defecto

Un `thread_local` C++ sin `constinit` y sin definición visible de inicialización constante se rechazaba:
`C++ thread_local needs the CRT on-demand TLS initialization (declare it constinit)`. Pasaba con las globales de
namespace y con los campos estáticos, en el camino directo y en una plantilla precompilada (el codec también lo
rechazaba). La tanda 10 lo tenía como control negativo (`tls-sin-constinit`).

## Lo que hace Clang

Con el destino `x86_64-pc-windows-msvc` y las guardas TLS del ABI de Microsoft, Clang 23.1.0 emite lo mismo antes
de cada acceso a una variable así declarada en otra unidad:

```llvm
%1 = load i8, ptr @__tls_guard
%2 = icmp eq i8 %1, 0
br i1 %2, label %init, label %next
init:
  call void @__dyn_tls_on_demand_init()
```

`__tls_guard` es un `thread_local` del CRT. `__dyn_tls_on_demand_init` (vcruntime) ejecuta los inicializadores TLS
dinámicos de la imagen en ese hilo y activa la guarda. Si alguna declaración es `constinit`, o la definición visible
tiene inicialización constante, Clang no emite la guarda. Es la condición que ya calculaba `ClangSession.cpp`
(`cppTlsOnDemand`). Comprobado con `tools/t19c/tls1/tlsprobe.cpp`, O1.

## Cambio

- **Proyección.** `cppTlsOnDemand` llega al símbolo:
  - en las globales: `CppSemanticGlobal::tlsOnDemand` y `NamespaceStateSymbol::cppTlsOnDemand`. Si varias
    declaraciones de la misma global difieren, basta una sin `constinit`: la guarda no cambia un almacenamiento ya
    inicializado;
  - en los campos estáticos: `CppSemanticStaticField::tlsOnDemand`, `CppImportedStaticField::tlsOnDemand` y
    `ClassMemberSymbol::cppTlsOnDemand`;
  - en el MIR: `ProjectOopMirStaticField::cppTlsOnDemand`, también en la identidad de caché del MIR.

  Ya no se rechaza. Una declaración C o sin `thread_local` con la marca es inconsistente y se rechaza. Un
  `thread_local` `dllimport` sigue rechazado: el ABI no lo permite, y Clang tampoco.
- **LLVM.** `staticFieldAddress` (`OopLlvmCalls.cpp`) llama, antes de dar la dirección, a
  `__nsc_cpp_tls_on_demand_guard`. Es una función interna `nounwind`, una por módulo, con la misma comprobación y la
  misma llamada que emite Clang. Con O1 o más, LLVM la integra en el sitio de acceso.
- **Catálogo precompilado** (`CppCRecordMetadataCodec.cpp`):
  - la marca final de una global C++ vale 1, o 2 si es `thread_local` bajo demanda;
  - el byte TLS de un campo estático vale 0, 1 o 2;
  - un catálogo sin estos casos conserva sus bytes.

  No hay versión nueva: el contrato vigente de NSMeta sigue siendo el único.

## Evidencia

Caso nuevo `tls-bajo-demanda` de `pruebas-tanda-19-traslados`: globales de namespace (un `int` y un record) y un
campo estático `thread_local`, inicializados con una llamada. NativeSharp los lee y los escribe, y la biblioteca
también.

| Prueba | Resultado |
|---|---|
| Estática con `--crt-startup`, O0–O3 | 7178, igual que Clang |
| Plantilla precompilada, con y sin encabezado | 7178 |
| IR de NSC | tiene la guarda (`__tls_guard`, `__dyn_tls_on_demand_init`), como el IR de Clang |
| `tls-global` y `tls-campo` (`constinit`) | sin guarda, como Clang |
| DLL | no aplica: Clang rechaza el `thread_local` `dllimport` |

En el hilo principal, el callback TLS del CRT ya inicializa las variables antes de `main`. Por eso el resultado
coincide aunque no hubiera guarda, y la presencia de la guarda se comprueba en el IR.

`pruebas-tanda-10-globales`: el negativo obsoleto `tls-sin-constinit` pasa a ser una comparación positiva con
Clang. El programa arranca por el CRT, la guarda está en el IR de los dos y el resultado es igual. Siguen los
rechazos genuinos: el campo `const` no se asigna y el record con destructor no trivial no se importa por valor.

Registros: `logs/fx-td2*.log` y `logs/fx-td3*.log`.

Regresión dirigida (`lista-td3.txt`: precompilado y caché de la tanda 9, tandas 10 a 18):

- 68 de 68 en estática (`logs/regresion-td3.log`);
- 47 de 47 en DLL (`logs/regresion-dll-td3d.log`);
- `lote-ct2`: `ninja -k 0` sin errores; las nueve CTests de interoperabilidad y `nsc_cpp_c_identity_tests` pasan;
- `ctest -R "snapshot|precompiled|record|cpp-interop|member|tls|global"`: 32 de 33, falla
  `nsc.cpp-interop.session` sin sus fixtures (T19-037);
- el fixture completo de traslados, 15 casos (en DLL, 12 ejecutados y 3 rechazos de Clang del `thread_local` `dllimport`), en estática y en DLL (`logs/fx-td3t.log`, `logs/fx-td3td.log`).

## Sigue pendiente

- Un hilo creado por NativeSharp que usa un `thread_local` C++ (T19-020).
- El diagnóstico propio de TLS sin las bibliotecas del CRT (T19-034).

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
