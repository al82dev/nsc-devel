# 142 — 1.0-E8: endurecimiento del compilador

**Fecha:** 2026-09-07  
**Entrega:** primera megaentrega E8  
**Stage:** 61  
**Estado:** implementado, instalado y certificado por G00

## Resultado

E8 empieza convirtiendo tres supuestos de robustez en contratos ejecutables:

1. una entrada muy dañada no puede hacer crecer sin límite la lista de diagnósticos;
2. una excepción C++ inesperada no puede salir de `main` como terminación muda;
3. la versión de LLVM deja de ser “alguna suficientemente nueva” y pasa a ser una línea calificada
   y observable.

La implementación no cambia la semántica de los programas válidos. Endurece el comportamiento del
compilador cuando el fuente, el entorno o el propio compilador fallan.

## 1. Presupuesto de diagnósticos

### El problema

`DiagnosticEngine` conservaba todos los diagnósticos que recibía. Eso era correcto para programas
pequeños, pero una entrada hostil o una recuperación desafortunada del parser podía acumular miles
de mensajes derivados del mismo daño inicial.

Una cascada sin límite tiene tres costos:

- memoria proporcional al número de síntomas, no al número de causas útiles;
- salida difícil de leer;
- tiempo perdido renderizando mensajes posteriores que rara vez ayudan.

### La regla nueva

El límite predeterminado es 100 errores. Se conservan los primeros 100; después se suprimen errores,
warnings y notas adicionales de esa compilación. La supresión no es silenciosa: el renderer informa
cuántos diagnósticos omitió.

Ejemplo contractual:

```text
nsc: error: diagnostic limit reached after 100 errors; 9 additional diagnostics suppressed
```

### Métodos añadidos

En `DiagnosticEngine`:

- `kDefaultErrorLimit` fija el presupuesto público;
- `errorLimit()` permite inspeccionarlo;
- `errorLimitReached()` distingue una compilación truncada de una que produjo exactamente pocos
  errores;
- `suppressedDiagnosticCount()` conserva la magnitud de la cascada;
- `suppress()` centraliza la decisión para las dos sobrecargas de `report()`;
- `clear()` reinicia también el estado del límite.

La prueba no se limita a llamar el engine directamente. Compila un buffer de 512 caracteres
inválidos y verifica 100 errores retenidos más 412 diagnósticos contabilizados como suprimidos.

## 2. Frontera de fallos internos

### El problema

`main` llamaba al driver directamente. Un `std::exception`, `std::bad_alloc` o una excepción ajena a
la jerarquía estándar podía atravesar el punto de entrada y terminar el proceso sin respetar el
contrato de salida de `nsc`.

Eso no convierte un error del usuario en un error interno. Los caminos normales siguen devolviendo
1 o 2 con sus diagnósticos. La frontera sólo actúa cuando un defecto inesperado escapó de todas las
capas anteriores.

### La solución

`runWithCrashBoundary()` envuelve la invocación completa, incluido el supervisor de LLVM:

- conserva intacto el valor de una ejecución normal;
- traduce `std::bad_alloc` a `out of memory`;
- conserva `what()` para una excepción estándar;
- produce un mensaje determinista para una excepción desconocida;
- devuelve siempre `ExitCode::InternalError`, cuyo valor es 70.

La función es pequeña y reutilizable a propósito. Stage 61 puede inyectarle lambdas que lanzan cada
familia de excepción y comprobar el contrato sin necesitar provocar corrupción real del compilador.

## 3. LLVM calificado y observable

### Lo que reveló la primera prueba

La primera versión de esta entrega asumió LLVM 20.x basándose en el mínimo histórico de la API. La
suite leyó la biblioteca cargada mediante `LLVMGetVersion` y respondió:

```text
found 23.1.0
```

El hallazgo corrigió el diseño antes de declararlo verde. “La API apareció en LLVM 20” y “esta es la
línea calificada para el producto actual” son afirmaciones diferentes. El producto certificado usa
LLVM 23.1.0, por lo que NativeSharp 0.9 fija ahora su contrato en LLVM 23.x.

### Dos niveles de comprobación

CMake rechaza un paquete LLVM anterior a 23 o posterior/igual a 24. Cuando el entorno sólo ofrece
una biblioteca monolítica y no hay metadata de paquete, `LlvmVersion.cpp` consulta la biblioteca
realmente cargada y la verificación de módulos se niega a continuar si el major no es 23.

Esto evita dos errores distintos:

- configurar conscientemente con una versión no calificada;
- compilar contra una instalación y cargar otra en ejecución.

El nuevo comando público muestra ambos datos:

```text
nsc --print-llvm-version
LLVM 23.1.0 (supported: 23.x)
```

## 4. Verificación de LLVM antes de avanzar

`LlvmModule::verify()` comprueba primero la versión cargada y después llama
`LLVMVerifyModule(..., LLVMReturnStatusAction, ...)`. El lowerer de proyectos aplica el mismo gate
antes de construir el módulo físico.

El verificador no aborta el proceso. Devuelve el detalle al driver para que un módulo roto termine
como error interno y no llegue a optimización, emisión o enlace.

## 5. Un contrato separado para el compilador

E7 formalizó el lenguaje. E8 agrega un documento distinto porque una regla de sintaxis no debe
mezclarse con políticas de fallo del ejecutable.

`NativeSharp_Compiler_Contract_1_0.md` contiene ocho cláusulas:

- `NSC-FAIL-001`: frontera de excepciones inesperadas;
- `NSC-DIAG-001`: presupuesto de diagnósticos;
- `NSC-DIAG-002`: ubicación y causa en mensajes de fuente;
- `NSC-IR-001`: verificación obligatoria del módulo;
- `NSC-LLVM-001`: LLVM 23.x;
- `NSC-OPT-001`: equivalencia observable O0/O2;
- `NSC-EXIT-001`: clases de exit code;
- `NSC-NEG-001`: evidencia negativa obligatoria.

El mapa paralelo conecta cada cláusula con implementación y pruebas. Stage 61 exige cinco campos,
IDs únicos, estado implementado, encabezado normativo y existencia de cada ruta citada.

## 6. Corrección al canario histórico de E7

Al avanzar a Stage 61, `nsc.stage60.language-contract` falló porque exigía que el stage actual fuera
exactamente 60. Eso confundía el punto de introducción de E7 con una versión que jamás podría
avanzar.

Ahora la suite convierte `kCompilerStage` a entero y exige `stage >= 60`. Sigue fallando si E7 se
ejecuta contra un compilador anterior, pero permite que E8 y entregas posteriores conserven el
contrato. La identidad exacta del snapshot continúa bajo `nsc.version.identity`.

## Archivos nuevos

- `include/nativesharp/Driver/CrashBoundary.h`;
- `include/nativesharp/LLVM/LlvmVersion.h`;
- `src/LLVM/LlvmVersion.cpp`;
- `tests/unit/Stage61CompilerHardeningTests.cpp`;
- `spec/NativeSharp_Compiler_Contract_1_0.md`;
- `spec/NativeSharp_Compiler_1_0_conformance.map`;
- este documento.

## Archivos modificados

- `CMakeLists.txt`: Stage 61, LLVM 23.x, nueva fuente y nueva suite;
- `include/nativesharp/Basic/Version.h`: stage 60 → 61;
- `include/nativesharp/Diagnostics/DiagnosticEngine.h`: estado y API del presupuesto;
- `src/Diagnostics/DiagnosticEngine.cpp`: aplicación del límite;
- `src/Diagnostics/DiagnosticRenderer.cpp`: resumen de supresión;
- `src/nsc/main.cpp`: frontera en el punto de entrada;
- `include/nativesharp/LLVM/LlvmModule.h`: elimina una declaración duplicada accidental;
- `src/LLVM/LlvmModule.cpp`: gate de versión antes del verificador;
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: gate antes del lowering físico;
- `src/Driver/Driver.cpp`: `--print-llvm-version` y ayuda;
- `tests/unit/Cc1BuildIdentityTests.cpp`: identidad Stage 61;
- `tests/unit/Stage60LanguageContractTests.cpp`: umbral histórico correcto;
- `spec/NativeSharp_1_0_conformance.map`: Stage 61 refuerza la evidencia diagnóstica;
- roadmap e índice documental.

## Evidencia local

La suite propia de E8:

```text
nsc.stage61.compiler-hardening
PASS: 95 checks
```

La selección focal ejecutó diagnóstico, lexer, parser, robustez arbitraria, contrato E7, contrato
E8, identidad, lowering LLVM, optimización LLVM y driver:

```text
lld-link tree  10/10 PASS
NSLink tree    10/10 PASS
```

Los dos árboles se reconstruyeron completos antes de ejecutar la selección. Se mantuvieron en
directorios independientes para no mezclar artefactos ni atribuir resultados de un enlazador al
otro.

La matriz completa quedó cubierta así:

```text
lld-link: pruebas 1..182       182/182 PASS
lld-link: pruebas 183..299     117/117 PASS
NSLink:   pruebas 1..299       299/299 PASS
```

La división de `lld-link` conserva un hallazgo útil. Su primera corrida completa dio 298/299: el
único rojo fue `nsc.version`, porque CTest todavía apuntaba deliberadamente al Stage 60 certificado
en `C:/native-sharp/bin/nsc.exe`. No era lícito cambiar el test para que aceptara una versión vieja
ni reemplazar el baseline vivo durante una validación local. Los dos árboles se reconfiguraron con
`NSC_TEST_COMPILER` apuntando al Stage 61 instalado en el prefijo aislado; el tramo completo que usa
el compilador instalado se repitió y cerró 117/117. NSLink ejecutó después los 299 casos de una sola
vez contra ese mismo producto y cerró sin fallos.

### Auditoría del diff antes del cierre

Durante la preparación se detectó que una invocación de formato había tocado miles de líneas del
lowerer sin cambiar su comportamiento. Esas modificaciones mecánicas se retiraron antes de la
validación final. El resultado revisable quedó reducido a cinco líneas nuevas en
`ProjectOopLlvmLowerer.cpp` y quince en `Driver.cpp`; después de esa limpieza se recompilaron ambos
árboles y se repitieron las pruebas focales. Esto importa porque un test verde no vuelve auditable
un diff innecesariamente grande.

## Instalación aislada

El producto final se instaló bajo `_targeted/e8-stage61-final`, sin reemplazar el Stage 60
certificado.
La identidad resultante es:

```text
nsc 0.9.0+build.20260907T222850Z.cca04993928e969b
NativeSharp 0.1, stage 61
LLVM 23.1.0 (supported: 23.x)
SHA-256 B385418E3AD85C98A486F42613DCE957424B3CEECCE7F7961EF1ABD663258C0A
```

La instalación contiene los dos documentos nuevos bajo `share/nativesharp/spec`. Sus SHA-256
coinciden con las fuentes:

```text
NativeSharp_Compiler_Contract_1_0.md
07F2537AA0BB6AA896F795A8CE305A317171A326763A7C9544DF241AA1AE509B

NativeSharp_Compiler_1_0_conformance.map
D523AD5B86B5C0CA7D6B6A037B6658DA751C727938FA600A5349377DC0B0EFD9
```

## Estado de cierre

Esta primera megaentrega E8 está cerrada en Stage 61. G00 la promovió con el run
`20260907-170412-16b5b8cb`: 11 etapas operativas, 324 casos, cero fallos, árbol inmóvil y ejecutable
construido idéntico al instalado. El acta y las identidades exactas están en el documento 143.
