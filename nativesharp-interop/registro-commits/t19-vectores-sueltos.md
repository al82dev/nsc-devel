# Tanda 19: vectores de 128 bits sueltos y genéricos NativeSharp junto a C++ (T19-036, T19-025)

## Defectos

1. **Vector suelto.** Un vector de Clang de 128 bits (`float` x4, `int` x4, `long long` x2) como parámetro o
   resultado de una función C o C++ no se importaba. `T::scale(f4, float)` daba
   `C/C++ vector return type is not yet mapped`.
2. **Genéricos NativeSharp con una referencia C/C++.** Un proyecto con una `CppLibraryReference` que usaba un tipo
   genérico NativeSharp no compilaba. Esto incluye `Span<T>` de NativeSharp.Core y cualquier `Holder<int>` propio.
   El error era `record observation name lookup failed`, sin ubicación.
   - Con NativeSharp.Core: `unknown type name 'Span' in module 'NativeSharpCore'` (`ArenaAllocator.ns:286`).
   - Con un genérico propio: `unknown type name 'Holder'`.
3. **Plan de Clang de un vector suelto.** `copyPlan` (`ClangAbiPlan.cpp`) solo representaba `<2 x float>` como
   valor vectorial de nivel superior. Un vector de 128 bits dejaba la declaración sin plan.
4. **Llamada a la importación.** La declaración se construía con el plan, pero `cVectorPlanValues` descartaba
   toda importación C++ por tener `cppObjectSizeModes` no vacío. Ese vector trae un cero por parámetro.
   La llamada usaba `sret` y la declaración no, y el verificador de LLVM rechazaba el módulo.
5. **Alias C/C++ de un productor precompilado** (apareció en la regresión al copiar los árboles). Las
   especializaciones copiadas de una plantilla precompilada nombran alias C/C++ del productor
   (`T09Cpp.Callback`). La búsqueda de la observación recibía solo los alias de las sesiones vivas, así que 330
   validadores precompilados de la tanda 9 fallaban con `unknown type name 'T09Cpp.Callback'`.

## Causa

- **Genéricos (2).** La observación de records C++ (`CppSemanticRecord.cpp`) corre después de la fase genérica.
  Esa fase declara las especializaciones y renombra los usos genéricos dentro de los árboles. Antes de este
  cambio, `ProjectCompilation::cloneSyntaxForObservation` volvía a analizar los tokens, lo que traía de vuelta los
  usos abiertos (`Span<T>`, `Holder<int>`). La búsqueda de nombres nunca declara los tipos genéricos
  (`predeclareNode` los omite), así que la búsqueda de la observación fallaba.
- **Vectores (1, 3, 4).** El portador NativeSharp (Float32x4, Int32x4, Int64x2) es un struct ordinario de 16 bytes.
  Sin el plan, la posición se trataba como un struct en memoria.

## Cambio

- `ProjectCompilation.cpp`, `SyntaxProductCacheCodec.*`: la observación copia el árbol ya reescrito de cada unidad
  (`copySyntaxTree`, con el codec de árboles del caché) en lugar de volver a analizar los tokens. Las unidades
  precompiladas ya se copiaban así.
- `CppSemanticRecord.cpp`:
  - la búsqueda de la observación recibe los alias de la búsqueda principal
    (`ProjectCompilation::cppImportedTypeAliases()`), que incluyen los de los catálogos precompilados;
  - su error incluye el primer diagnóstico con su ubicación.
- `ClangAbiPlan.cpp`: con la convención C de Win64 (`cdecl`, `ms_abi`) o `__vectorcall`, un vector de 128 bits de
  nivel superior se representa en el plan. Las demás convenciones (`sysv_abi`, `regcall`, `preserve_*`) y los
  vectores de 256 y 512 bits siguen fuera, como comprueba `nsc.cpp-interop.clang-abi-probe` con `sysv_abi pv`.
- `ClangSession.cpp`, `CppSemanticInventory.cpp`: las funciones C++ y C con un vector por valor (`cdecl`, `ms_abi`,
  `__vectorcall`) conservan el plan de Clang de su declaración.
- `CppSemanticFunction.cpp`:
  - `cppVectorValueType` proyecta cada vector a su portador de NativeSharp.Core.Numerics.
  - Exige que el plan lo pase directo, como vector de los mismos carriles.
  - Con `__vectorcall` solo admite `float`.
  - Sin NativeSharp.Core, el diagnóstico pide el portador.
- `MirFunctionLowerer.cpp`: un resultado vector directo en el plan no se devuelve en memoria.
- `OopLlvmDeclarations.cpp`, `OopLlvmCalls.cpp`, `OopLlvmTypesAbiAttributes.cpp`:
  - la declaración y la llamada usan vectores LLVM en esas posiciones;
  - el portador se convierte de y a vector;
  - `cVectorPlanValues` solo descarta una importación que pide un tamaño de objeto oculto.
- `CppCRecordMetadataCodec.cpp`: el catálogo precompilado transporta el plan de esas funciones, sin versión nueva
  de NSMeta.

## Evidencia

| Caso | Contenido | Resultado |
|---|---|---|
| `vector-suelto` (`pruebas-tanda-19-traslados`) | C++: `f4`, `i4`, `l2` como parámetro y resultado `cdecl`, `f4` con `__vectorcall`; consumidor con NativeSharp.Core | 124896, igual que Clang, estática y DLL O0–O3; 7 declaraciones con la forma de Clang; plantilla precompilada con y sin encabezado |
| `validar-vectores-c.py` (nuevo) | lo mismo con una biblioteca C (`Language="C"`) | 124896, igual que Clang, estática y DLL O0–O3; 7 declaraciones con la forma de Clang |
| `generico-nativo` | `Box<int>`, `Box<long>` y `Shelf<T.Rec>.Pick` (método de clase genérica con un record C++) junto a la referencia C++ | 15645, igual que Clang, estática y DLL O0–O3; plantilla precompilada que publica los genéricos, con y sin encabezado |

Registros:

- `logs/fx-vs3*.log` (`vector-suelto`);
- `logs/fx-gn1*.log` (`generico-nativo`);
- `logs/vc1*.log` (vectores C);
- `logs/probe3.log`: reproducción mínima del defecto 2.

Regresión (`lista-vs6.txt`: tandas 9 a 18, porque el cambio toca la observación de records de todo proyecto
C/C++ y el plan de Clang):

- estática: 1154 de 1160 pasan y 2 se omiten (requieren mutador). Fallan 4:
  - las dos `cpp-variadica-preserve-none` (T19-032);
  - `cpp-tamano-objeto/validar-indice-expresion` y `validar-puntero-desplazado`, que fallan bajo carga (T19-026) y
    pasan repetidas con un proceso (`logs/regresion-t09-vs6r.log`);
- DLL: 61 de 61 (`logs/regresion-dll-vs6d.log`);
- el fixture completo de traslados (20 casos) pasa en estática y DLL (`logs/fx-vs6t.log`, `logs/fx-vs6td.log`);
- los guiones de la tanda 8 (vectorcall C, callbacks C e identidad de funciones C) pasan en estática, DLL y
  precompilado;
- `lote-ct2`: `ninja -k 0` sin errores; las nueve CTests de interoperabilidad y la de identidad pasan;
- `ctest -R "snapshot|precompiled|record|cpp-interop|member|callback|delegate"`: 33 de 35, las mismas dos fallas de
  la línea base (`nsc.cpp-interop.session`, T19-037; `nsc.stage42.delegates-events`).

La primera corrida (`logs/regresion-vs5.log`) dio 330 fallas en la tanda 9, todas por el defecto 5. También falló
`nsc.cpp-interop.clang-abi-probe`, porque el plan representaba el vector de `sysv_abi pv`. Ambas se corrigieron
antes de esta corrida.

## Sigue pendiente

- Vectores de 256 y 512 bits: requieren un nivel de CPU AVX por proyecto (registro
  `t19-vectorcall-avx-verificacion.md`).
- `double` x2 suelto (`__m128d`) se rechaza con `its lanes have no NativeSharp.Core.Numerics carrier`:
  NativeSharp.Core no tiene un portador `Float64x2`.
- Un struct NativeSharp con un campo C++ (`Box<T.Rec>` con un campo `U`) da
  `C/C++ type 'T.Rec' has no imported object layout yet`, también sin genéricos. Está en T19-015.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
