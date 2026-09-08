# 153 — E11: lambdas y clausuras léxicas compartidas / Stage 67

**Fecha:** 2026-09-08

**Versión compatible:** `nsc 0.9.0`

**Fotografía del compilador:** Stage 67
**Estado:** implementada, instalada y certificada por G00

## 1. Qué cerró esta entrega

Antes de E11, NativeSharp tenía delegates y una primera implementación útil de lambdas, pero esa
implementación todavía era deliberadamente pequeña:

- una captura era una copia tomada al crear cada delegate;
- dos lambdas que nombraban la misma local recibían dos valores independientes;
- escribir una variable capturada no tenía un slot común que modificar;
- una lambda dentro de otra se rechazaba;
- los parámetros de lambda sólo podían escribirse como identificadores sin tipo;
- el contexto sólo transportaba valores de una palabra de máquina;
- toda lambda se rechazaba al convertirla a `extern C delegate`, incluso sin captura.

Eso servía para demostrar la tubería AST → Sema → HIR → MIR → LLVM, pero no constituía todavía
una semántica de clausuras léxicas. E11 reemplaza ese modelo por uno coherente: la variable
capturada tiene una identidad única y todas las clausuras que la alcanzan comparten su celda.

## 2. Semántica visible

### 2.1 Captura compartida, no fotografía

```csharp
int state = 1;
Transform read = value => state + value;
Transform write = value => { state = value; return state; };
state = 4;
int before = read(1); // 5
write(10);
int after = read(2);  // 12
```

`state`, `read` y `write` apuntan al mismo almacenamiento. La asignación posterior a crear las
lambdas es visible; la escritura de `write` también. No hay una copia privada por delegate.

### 2.2 Lambdas anidadas y capturas transitivas

```csharp
Transform Make(int methodSeed)
{
    Factory outer = (int outerSeed) =>
    {
        int localSeed = 3;
        return (int value) => value + outerSeed + localSeed + methodSeed;
    };
    return outer(2);
}
```

La lambda interior distingue tres orígenes:

- `outerSeed`: parámetro de la lambda padre;
- `localSeed`: local creada dentro del cuerpo padre;
- `methodSeed`: parámetro del método, ya transportado por el entorno padre.

El binder conserva tanto la identidad original como el lugar desde el cual la lambda hija obtiene
la celda. Así evita duplicar `methodSeed` y permite cualquier profundidad de anidamiento con el
mismo mecanismo.

### 2.3 Parámetros contextuales o explícitos

Siguen siendo válidas las formas familiares:

```csharp
Transform a = value => value + 1;
Transform b = (value) => value + 1;
Transform c = (int value) => value + 1;
Mutate d = (ref int value) => { value = value + 1; };
```

En las dos primeras, el delegate destino aporta el tipo. En las dos últimas, el texto escrito es
un contrato: tipo y modo de paso deben coincidir exactamente con la firma del delegate.

Un tipo explícito también participa en overload resolution. Si dos overloads reciben delegates
con parámetros distintos, `(int value) => ...` puede seleccionar el de `int`; una lambda sin tipos
permanece ambigua cuando ambos destinos son igualmente posibles.

### 2.4 Valores mayores que una palabra

La celda se dimensiona con el layout real del tipo. E11 admite capturar:

- enteros, floats, enums, referencias, strings y delegates;
- structs pequeños o grandes;
- arrays nativos como referencia;
- fixed arrays inline, respetando tamaño y alineación.

El antiguo límite de ocho bytes desaparece de las clausuras. No se empaqueta un struct grande en
un entero ni se conserva por accidente una dirección temporal.

### 2.5 Frontera C

Una lambda sin captura puede convertirse a `extern C delegate`: el backend entrega un function
pointer con el thunk de protección de excepciones ya usado por callbacks NativeSharp. Una lambda
con captura se rechaza con un diagnóstico específico porque un puntero C no dispone de un canal
para transportar el entorno.

## 3. Representación interna

Cada variable capturada se aloja en una celda de heap:

```text
+----------------------+  offset 0
| refcount atómico i64 |
+----------------------+  offset alineado
| valor T              |  tamaño y alineación reales
+----------------------+
```

El entorno de una lambda ya no contiene `T`; contiene un puntero a la celda de `T`. Por ello dos
entornos distintos pueden compartir una variable sin compartir necesariamente las demás.

La activación creadora inicia con una referencia. Cada entorno que conserva la celda ejecuta
`__nativesharp_capture_cell_retain`. El destructor específico del entorno ejecuta un release por
slot y después libera el propio entorno. El último release libera la celda.

La activación creadora suelta sus referencias:

- en cada `return` normal;
- al alcanzar el final de una función `void`;
- mediante un `cleanuppad` de función cuando una excepción sale del método.

Una excepción capturada dentro del mismo método no libera esas referencias antes de tiempo: el
método sigue vivo y el bloque `catch` puede continuar leyendo o escribiendo las variables.

## 4. Cómo atraviesa el compilador

### Parser

`Parser::looksLikeLambdaExpression` reconoce ahora una lista balanceada seguida de `=>`, no sólo
una lista de identificadores. `Parser::parseLambdaExpression` reutiliza el parser normal de
parámetros cuando encuentra tipo o modo de paso y conserva la forma contextual para `(x, y)`.

### Modelo de cuerpos y Sema

`ProjectMethodBodyModel::bindLambdaToDelegate`:

- asigna ordinales globalmente únicos durante el análisis del método;
- mantiene una pila de lambdas activas;
- valida tipo y modo explícitos;
- registra tipos de valor para parámetros por referencia;
- permite que el tipo explícito filtre candidatos de overload.

`ProjectMethodBodyModel::collectLambdaCaptures` separa dos preguntas que antes estaban mezcladas:

1. ¿qué declaración representa esta variable?;
2. ¿desde dónde obtiene su celda la lambda actual?

La respuesta a la segunda se codifica como local, parámetro o `this` originales; local o parámetro
del padre; o captura ya existente del padre. El mapa por binding impide que el cuerpo exterior se
apropie de nombres utilizados exclusivamente por una lambda interior.

### HIR y MIR

Se añadieron tres operaciones explícitas en ambos niveles:

- `CaptureCellReference`: obtiene la celda creada por la activación actual;
- `ClosureCaptureCellReference`: obtiene una celda transportada por el entorno padre;
- `ClosureCaptureAddress`: obtiene la dirección del valor dentro de la celda para una escritura.

La metadata de cada función enumera locales y parámetros que deben vivir en celdas y si corresponde
crear una celda para `this`. HIR conserva el origen semántico; MIR conserva la operación exacta que
el backend debe emitir.

### LLVM

`ProjectOopLlvmLowerer::allocateCaptureCell` calcula el layout, reserva cabecera más payload y
devuelve por separado el puntero de celda y la dirección alineada de `T`.

`ensureCaptureCellRuntime`, `retainCaptureCell` y `releaseCaptureCell` producen el lifetime
atómico compartido. `closureContextDestroy` genera una función destructora por forma de entorno,
sin bucles ni metadata dinámica. `captureCellExceptionalCleanup` cierra la referencia del creador
en unwind. `lowerLambdaCreate` conserva punteros de celda en el entorno y usa el thunk C cuando el
delegate externo no captura estado.

## 5. Ownership: decisión deliberada

Capturar no significa copiar o transferir propiedad en secreto. Continúan rechazados:

- una local `raii`;
- una local o parámetro `borrowed`;
- una vista;
- un parámetro `owned`.

Permitirlos exigiría una sintaxis y una regla explícitas para préstamo o transferencia de la
captura. E11 no inventa una copia, no ejecuta `move` implícito y no prolonga un préstamo. Esta
restricción preserva el modelo de memoria del lenguaje mientras las capturas ordinarias ya tienen
semántica completa.

## 6. Archivos modificados

| Archivo | Cambio |
|---|---|
| `src/Parser/Parser.cpp` | reconocimiento y parseo de parámetros de lambda tipados |
| `include/nativesharp/Frontend/ProjectMethodBodyModel.h` | identidad de origen, ruta de captura, pila y metadata de parámetros |
| `src/Frontend/ProjectMethodBodyModel.cpp` | lambdas anidadas, captura transitiva, mutación, validación contextual y overloads |
| `include/nativesharp/Frontend/ProjectOopHirModel.h` | nodos y metadata de funciones con celdas |
| `src/Frontend/ProjectOopHirModel.cpp` | construcción de referencias/direcciones de celda y marcado de homes capturados |
| `include/nativesharp/Frontend/ProjectOopMirModel.h` | contrato MIR equivalente |
| `src/Frontend/ProjectOopMirModel.cpp` | transporte HIR → MIR de operaciones y metadata E11 |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | layout, retain/release, entornos, mutación y cleanup normal/excepcional |
| `tests/unit/Stage43LambdasClosuresTests.cpp` | actualización del contrato histórico, positivos E11 y negativos tipados |
| `tests/unit/Stage55XInteropFoundationTests.cpp` | lambda C sin captura positiva y captura C negativa |
| `tests/unit/Stage67E11LambdaExecutionTests.cpp` | fixture nuevo de ejecución O0/O2 |
| `spec/NativeSharp_Grammar_1_0.ebnf` | gramática contextual y explícita de parámetros lambda |
| `spec/NativeSharp_Language_Specification_1_0.md` | regla normativa de clausuras compartidas |
| `spec/NativeSharp_1_0_conformance.map` | trazabilidad completa de NS-DELEGATE-001 |
| `include/nativesharp/Basic/Version.h`, `CMakeLists.txt` | identidad Stage 67 y registro de la suite |

## 7. Evidencia focal

La suite histórica comprueba AST/Sema, HIR, MIR, LLVM válido, optimización O2 y objeto COFF. Los
casos E11 agregan:

- hermanas que comparten y mutan una local;
- lambda anidada con parámetro del método, parámetro del padre y local del padre;
- overload resuelto por parámetro explícito;
- rechazo por tipo y modo de paso incompatibles;
- struct mayor de ocho bytes;
- funciones retain/release y direcciones de payload visibles en LLVM;
- callback C sin captura y rechazo de callback C con entorno.

La fixture Stage 67 compila y ejecuta a O0 y O2. En una sola aplicación ejercita mutación
compartida, profundidad léxica, `ref`, struct de 16 bytes, fixed array de 16 bytes, destrucción
explícita de delegates devueltos, continuidad de una celda después de un `catch` local y cleanup
cuando una excepción abandona la activación creadora. Su salida contractual es `42`.

Resultados observados en el árbol final de la entrega:

- `nsc.stage43.lambdas-closures`: aprobado, 81 checks internos;
- `nsc.stage67.e11-lambda-execution`: aprobado, 4 checks; construcción y ejecución O0/O2;
- `nsc.stage55x.interop-foundation`: aprobado, 24 checks internos;
- identidad del compilador y conservación del contrato E9+E10: aprobadas;
- cuatro suites críticas de LLVM/SEH/RAII más el contrato normativo: 5/5 CTest;
- conjunto focal total ejecutado por CTest: 10/10 casos;
- `nsc.exe`: enlazado correctamente como `0.9.0`, Stage 67.

La implementación se cerró primero con evidencia focal. Después, G00
`20260908-092439-7d0bd312` reconstruyó el árbol, instaló el compilador y certificó Stage 67 con cero
fallos. El acta completa, incluidos hashes, tiempos y conteos, está en el documento 154.

## 8. Impacto de versión

E11 cambia la fotografía de compilador de Stage 66 a Stage 67 porque modifica sintaxis aceptada,
binding, representación intermedia y semántica de ejecución. La versión compatible permanece en
`0.9.0`: ésta es una entrega rumbo al contrato 1.0 y ya cuenta con promoción instalada propia.

El criterio de cierre de E11 quedó satisfecho: las pruebas focales quedaron verdes con LLVM válido,
la fixture devolvió 42 en O0/O2 y G00 certificó la fotografía Stage 67 completa.
