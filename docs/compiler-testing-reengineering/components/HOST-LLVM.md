# Host.LLVM — Lowering, optimización, objetos y frontera LLVM 23

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo inferencias y riesgos marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 40 archivos, 25,115 líneas y 1,269,325 bytes.
- **Inventario:** 802 símbolos físicos, 64 contratos y 512 casos diseñados.
- **Validación realizada:** lectura estática completa de archivos pequeños, indexación AST de los 40 archivos,
  revisión dirigida de los lowerers, extracción de ramas/errores y segunda auditoría documental.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.LLVM recibe MIR optimizado o MIR físico de proyecto y produce módulos LLVM u objetos COFF para Windows.
También concentra la política de optimización, target machine, depuración, particionado, cache de artefactos,
ThinLTO, sanitización y el subconjunto LLVM-C que usa NSC. No decide semántica del lenguaje: presupone modelos
Frontend/MIR ya construidos y rechaza estados incompletos.

La implementación contiene dos rutas de lowering:

1. `LlvmLowerer`, para el MIR genérico optimizado;
2. `ProjectOopLlvmLowerer`, para el MIR físico de proyectos, OOP, runtime, depuración y ABI completo.

## 3. Contratos pequeños y vocabularios

Los enums y parsers cubren `DebugInformationKind`, `FloatingPointMode`, FMA, rounding, denormals,
`LinkTimeOptimizationMode`, `LlvmOptimizationLevel` y `LlvmTrapReason`. Los nombres son contratos exactos. El
estado floating activo es thread-local y un scope restaura el estado anterior; los workers reciben una copia
explícita del estado capturado.

`LlvmTrapReason` conserva valores numéricos 1..16. Aunque el desbordamiento `MinValue / -1` ya no aborta, el
enumerador histórico no se renumera porque el código de trap es observable.

## 4. Frontera LLVM-C y versión

`LlvmCApi.h` mantiene a mano el subconjunto LLVM-C consumido por NSC: 20 tipos/enums físicos y 230 funciones
externas. Sus valores de enum, firmas, calling convention y disponibilidad pertenecen al contrato con LLVM 23.
La prueba futura debe contrastarlos contra headers oficiales de la misma instalación, no duplicar otra tabla
manual dentro del test.

`verifyLoadedLlvmVersion` exige major 23. El diagnóstico vigente todavía dice `NativeSharp 2.0`; se conserva como
hallazgo estático y no se corrigió en esta fase.

## 5. Módulo, target y emisión

`LlvmModule` posee contexto y módulo, los libera una sola vez, verifica IR y expone texto. `LlvmTargetMachine`
inicializa x86/ARM64 una vez, crea la máquina con triple/CPU/features, instala target y data layout y emite objeto.
La escritura reintenta sólo errores Windows compatibles con archivo mapeado o compartido: hasta 100 intentos de
10 ms. Otros errores no se ocultan.

`CoffObjectVerifier` exige header mínimo de 20 bytes, máquina AMD64, al menos una sección y optional-header size
cero. No valida el contenido completo de secciones.

## 6. Optimización, observación y sanitización

O0..O3 se traducen a `default<O*>`; el manifiesto es `NSOPTIMIZATION 1.0`. El optimizer verifica/configura salvo
en su entrada preparada. O0 puede omitir el pipeline si no hay inline obligatorio ni funclets SEH; funciones SEH
reciben `mem2reg`. La observación no debe cambiar el resultado.

El presupuesto sólo contabiliza duraciones positivas O2 y satura. El parser de trace reconoce pasos, frames,
probes y unidad actual, detecta repetición mayor a 64 y presupuesto de 120 segundos. `LlvmProcessTrace` ejecuta
O1/O2/O3 con captura de stderr; O0 no usa esa ruta.

El sanitizador nativo sólo implementa Address, añade el atributo a definiciones, ejecuta el pipeline con
verify-each y vuelve a verificar. Publica `clang_rt.asan_dynamic-x86_64` como runtime requerido.

## 7. Cache de artefactos

El cache activo es thread-local. Las claves incorporan toolchain, LLVM cargado, target/layout, política de
codegen, pipeline, debug e identidad IR/MIR. Son SHA-256 lowercase de 64 caracteres. Los objetos viven bajo
`objects/<prefijo>/<clave>.obj` con metadata `NSCOBJ 1`.

Restore comprueba clave, tamaño y hash. Entradas inválidas se ponen en cuarentena; store publica atómicamente y
usa locks por entrada. En Windows el lock espera hasta 3000 intervalos de 1 ms. Prune conserva las entradas más
recientes dentro del presupuesto. Errores individuales de enumeración pueden omitirse, pero el error terminal del
iterador se reporta.

## 8. Particionado e identidad MIR

El particionador es de un solo uso y exige MIR construido. Excluye funciones importadas C/native. `Balanced`
distribuye por coste con LPT; `StableIncremental` asigna por hash estable. La clausura de declaraciones agrega
llamadas, miembros, panic y destrucción automática. Las particiones vacías se omiten sin renumerar las claves
estables.

La identidad MIR v4 serializa schemas, layouts, funciones y nodos de forma determinista. `nodeCost`, la clausura
de destrucción, el recorrido de nodos y la identidad usan recursión existente; es un riesgo de implementación
registrado, no un cambio autorizado.

## 9. Generación de objetos

`ProjectOopObjectGenerator` elige módulo único para full debug con inline optimizado, inline obligatorio o Full
LTO. En los demás casos puede particionar; usa particiones estables cuando hay cache o ThinLTO, balanceadas en el
resto. El bundle temporal elimina su directorio en RAII y conserva objetos, IR, métricas, cache y ThinLTO.

El cache pre-LLVM usa identidad MIR cuando no hay debug y el modo no es ThinLTO; los demás usan cache post-LLVM.
Todo hit COFF se valida. Workers heredan el estado floating, cancelan al primer fallo y convierten excepciones de
worker en error. ThinLTO deriva su índice de hashes de módulos ordenados por ordinal estable.

## 10. Lowerer MIR genérico

El lowerer genérico verifica el módulo optimizado, declara structs, globals, funciones y hooks de trap/alloc/free,
emite cuerpos y verifica el módulo resultante. Modela parámetros, locals, phi diferidos, constantes, load/store,
aritmética, comparaciones, conversiones, punteros, campos, arrays, llamadas y control de flujo.

Emite guards para división por cero, shifts inválidos, bounds, conversión floating-entero fuera de rango y
overflow de tamaño de allocation. `SignedDivisionCorrection.h` corrige sin branch `MinValue / -1` seleccionando
divisor 1 y deja intacta la trampa por división entre cero. Strings no se aceptan como inicializador global.

## 11. Lowerer OOP: lifecycle, debug y runtime

El request distingue sin debug, con debug y partición. Requiere nombres/MIR construidos, contexto debug completo
cuando aplica y nombre de módulo no vacío para partición. La implementación publica contexto/módulo por
transferencia única.

La depuración crea compile unit, archivos con checksum, CodeView file numbers, subprograms, scopes, parámetros,
`this`, locals, globals y tipos derivados de layouts físicos. Los prefix maps usan path portable y matching
Windows case-insensitive con el prefijo más específico. El source language C API vigente es C#; se registra como
decisión de interoperabilidad, no como semántica del lenguaje.

La declaración runtime materializa hooks de consola, reloj, entorno, archivos, procesos, threads, atomics,
sincronización, memoria, excepciones y panic. Cada operación valida aridad, metadata y disponibilidad del hook.

## 12. Lowerer OOP: ABI, dispatch y llamadas

Static fields y type initializers separan declaración, storage y cuerpos. ODR/comdat conserva identidad de
especializaciones. El ABI aplica `sret`, `inreg`, alineación, carrier indirecto, vectorcall/HVA, atributos floating,
uwtable y calling convention.

Dispatch emite descriptors, vtables, interface maps, destruction entries, RTTI, virtual/interface targets y
typed catch filters. Runtime-bound imports resuelven DLL/symbol y callback guards contienen excepciones en la
frontera. C ABI wrappers adaptan delegates y aggregates sin cambiar el contrato interno.

## 13. Lowerer OOP: lifetime y excepciones

El lowerer reserva allocas en entry, conserva flags de ownership y construye cadenas de cleanup normales y
excepcionales. Using, RAII, drops de variantes/valores, owned parameters y destrucción de arrays comparten esa
maquinaria. Structured exit ejecuta sólo los cleanups que abandona y limpia el estado de excepción al salir de un
catch.

Try/catch/finally usa funclets Windows, filtros tipados, cleanup pads, rethrow y transferencias de panic/native.
Inline assembly separa líneas, inputs, outputs y clobbers, y valida arquitectura/features antes de emitir.

## 14. Lowerer OOP: expresiones y runtime intrinsics

`lowerExpression` es el dispatcher central. Cubre ownership, addresses, allocación, loads/stores, constantes,
arrays, strings, archivos/entorno/procesos, atomics, SIMD, sincronización, closures, delegates, events,
conversiones, llamadas, construcción, destrucción, assert/assume/unreachable/panic y propagación de resultados.
Cada helper valida forma MIR y falla con diagnóstico sin aceptar estado parcial.

Las operaciones compuestas incluyen capture cells con retain/release, closures, delegates owning, events,
aggregate equality/hash, nombres de tipo/enum, strings nativos, arrays rectangulares, slices, type tests/casts,
match, coalescing, short-circuit, named-argument permutation y must-tail.

## 15. Tipos, conversiones y memoria

El mapeo de type keys cubre builtins, enums, punteros, arrays, structs, classes, delegates, SIMD y carriers ABI.
Bit-fields validan ancho, offset y unidad, y extraen/insertan con máscaras. Conversiones distinguen enteros,
floating, punteros, tuples estructurales y lambdas no capturantes. Receiver checks y guards producen traps
estables.

## 16. Estrategia de prueba y reconstructibilidad

Los 64 contratos producen 512 casos: 448 `PR` y 64 `NIGHTLY`. Las unitarias futuras usarán MIR mínimo y dobles
controlados de LLVM donde el contrato sea propio de NSC; los contratos de ABI/emisión requieren además comparación
estructural de IR/COFF y una instalación LLVM 23 conocida. No se contará cada `if` manualmente: la fase de coverage
instrumental recalculará ramas sobre el código vigente.

Los 40 archivos, 802 símbolos, 64 contratos y 512 casos permiten reconstruir la frontera LLVM, ambos lowerers,
el cache, particionado y generación de objetos. La segunda pasada está en
`audits/TANDA-08-HOST-LLVM-AUDIT.md`.

**Resultado:** `RECONSTRUIBLE` para la revisión congelada, sin afirmar ejecución ni cobertura.
