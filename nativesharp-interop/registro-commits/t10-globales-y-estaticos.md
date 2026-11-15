# T10-004, T10-006, T10-014 y T10-015: globales y campos estáticos C++ de tipo record, enum, arreglo y thread_local

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md) fallaban:

- `A.origin.y`, con `extern P origin` y `P` un record C++ trivial:
  `C/C++ global type is not yet mapped`;
- `A.tl`, con `extern thread_local int tl`: `thread-local C/C++ global is not yet projected`;
- `A.S.table[2]` y `A.S.origin.y`, campos estáticos de tipo arreglo y record:
  `no compatible member named 'table'`.

Causas en el código:

- `mapGlobalType` (`CppSemanticInventory.cpp`) solo mapeaba un record por valor en una global C.
- `projectGlobalWithLookup` rechazaba todo `thread_local` que no fuera C.
- `projectCppSemanticStaticField` (`CppSemanticRecord.cpp`) solo mapeaba escalares y punteros.
  `ProjectMemberModel::setExperimentalCppStaticFields` exigía además un ancho escalar.

Al revisar el acceso TLS de Clang apareció una condición más. Clang lee un `thread_local` C++ a través
de `__tls_guard` y llama a `__dyn_tls_on_demand_init`, la inicialización bajo demanda del CRT, salvo que
una declaración sea `constinit` o la definición visible tenga inicialización constante. Lo muestra el
IR de Clang 23.1.0 para `extern thread_local int a` frente a `extern constinit thread_local int b`.

## Cambio

- `CppSemanticInventory.cpp`:
  - `mapGlobalType` mapea un record C++ por valor, también como elemento de un arreglo, si es un
    valor C++ trivial (`cppTrivialValueEligible`) con el tamaño y la alineación de Clang;
  - un `thread_local` C++ se proyecta como uno C si Clang no lo protege con la inicialización bajo
    demanda. Si la necesita, se rechaza con un diagnóstico propio que pide `constinit`.
- `CppGlobalDeclaration.h` y `ClangSession.cpp`: `cppTlsOnDemand` aplica la regla de Clang. Una global
  `thread_local` C++ necesita el CRT si ninguna declaración es `constinit` y la definición visible no
  tiene inicialización constante.
- `CppSemanticRecord.cpp`:
  - `projectCppSemanticStaticField` mapea campos estáticos de enum o de record C++ trivial, y arreglos
    fijos de escalares, enums o esos records;
  - el selector de tipos importa con el record los enums de sus campos estáticos y de los parámetros
    y resultados de sus métodos estáticos, como ya hacía con los campos de instancia.
- `ProjectMemberModel.cpp`: un campo estático de tipo importado o arreglo no necesita ancho escalar; al
  construir los miembros se comprueba que su tipo sea visible.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia.

## Evidencia

Fixture nuevo `pruebas-tanda-10-globales/validar.py`, con y sin `--dll`. Encabezado:

- globales `origin` (record), `path[2]` (arreglo de records), `grid[2][3]` y `level`
  (`enum class Level : short`);
- `struct S` con los campos estáticos `table[3]`, `corner` (record), `mode` (enum) y `const P fixed`;
- solo en estática: `extern constinit thread_local int tls` y `constinit thread_local P tlsPoint`.

El consumidor lee todas las formas, escribe campos de records, elementos de arreglos y el enum, y
llama a `bump()`, que modifica `origin` y `S::table` en la biblioteca. Con thread-locals también los
escribe y llama a `tlsRead()`.

De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp. Resultado: 133 en
  estática, 119 en DLL, donde no hay thread-locals;
- exige las declaraciones de función del IR de NSC con los tipos físicos de Clang;
- consume una plantilla precompilada, con y sin encabezado, que llama a `bump()`.

El ejecutable con thread-locals enlaza el soporte TLS del CRT (`libcmt.lib`) con `lld-link`, como las
pruebas TLS de C existentes. Una sonda (`pruebas-tanda-10-globales/sonda-tls.py`) muestra que con NSLink el
ejecutable no tiene directorio TLS ni para C ni para C++ (`A:/temp/nsc-sonda-tls-_b2hq6pg`). Con
`lld-link`, una global C `_Thread_local` y una C++ `constinit thread_local` devuelven lo esperado
(`A:/temp/nsc-sonda-tls-kix6utma`).

Controles negativos y su contraparte:

- escribir `T10.S.fixed.x` en un campo estático `const` se rechaza (`not assignable`); la contraparte
  lo lee;
- un campo estático de un record con destructor no trivial no se importa
  (`no compatible member named 'value'`); la contraparte escribe uno trivial;
- `extern thread_local int dyn` sin `constinit` se rechaza
  (`C++ thread_local needs the CRT on-demand TLS initialization`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-globales-uyh_72q5` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-globales-meaybcmd` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Un `thread_local` C++ sin `constinit` ni definición constante visible necesita
  `__dyn_tls_on_demand_init`, que NSC aún no emite.
- Los campos estáticos `thread_local` no se importan.
- Una global C++ de tipo enum exige que el programa nombre el enum; los de los miembros estáticos se
  importan con su record.
- Las globales y los campos estáticos C++ siguen sin reconstruirse en el cuerpo de una plantilla
  precompilada.
- NSLink no escribe el directorio TLS: los ejecutables con thread-locals se enlazan con `lld-link`.
