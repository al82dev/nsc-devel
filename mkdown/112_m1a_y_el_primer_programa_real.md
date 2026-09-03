# 112 -- M1-A cerrada, y el primer programa real

Fecha: 2026-09-03
Ámbito: solo el compilador (`A:\nsc-devel`).
Documentos hermanos: `110_contrato_de_overflow.md`, `111_contrato_de_memoria.md`.

Dos entregas en una: la **fase M1-A** del plan de continuación (contratos, deuda medida,
diagnósticos) y el **primer programa NativeSharp que abre una ventana**.

---

# Parte A — M1-A

## A.1 ID 130 (`uwtable`): medido y arreglado

Estaba vivo. `applyUwtableAttribute` creaba el atributo con **valor 0**, y `uwtable` no es un
atributo booleano: su valor es un `UWTableKind`, donde **0 significa `None`** — o sea, "esta función
no tiene tabla de desenrollado", exactamente lo contrario de lo que el método dice hacer.

**Medido** contra `libLLVM-18` con la propia `src/LLVM/LlvmCApi.h` del proyecto y **sin el shim del
contenedor**, que reescribía 0→2 y por tanto tapaba el síntoma. `kindId` de `uwtable` es 87:

```
valor 0  ->  NADA. Ni linea `attributes #0`, ni `#0` en el `define`. El modulo impreso sale
             degradado: LLVMPrintModuleToString devuelve un texto sin la definicion de la funcion.
valor 1  ->  attributes #0 = { uwtable(sync) }
valor 2  ->  attributes #0 = { uwtable }          <- Async, la forma por omision
valor 3  ->  attributes #0 = { uwtable(async) }
```

**Se elige 2** (Async, se imprime `uwtable` a secas): es lo que clang-cl emite en Windows x64,
donde las tablas asíncronas son el comportamiento por omisión.

**Una advertencia sobre el alcance de la medición, porque importa.** El contenedor usa LLVM 18 y el
`nsc` de Windows enlaza `LLVM-C.lib` de `C:\clang-llvm`, que es **LLVM 23** (`01_build.log`: *"Clang
23.1.0"*). La correspondencia valor→`UWTableKind` es del formato del IR y no cambia entre versiones,
pero como cada una trata el 0 a su manera, el efecto **observable** puede diferir — y eso explica que
en el contenedor el módulo saliera roto y en Windows el binario construido pareciera correcto (el
comentario del código citaba un `uwtable(sync)` visto en un binario de Windows, que con valor 0 es
imposible en LLVM 18). Por eso el arreglo se hace igual, y por eso **la prueba tiene que mirar el IR
en Windows** además de ejecutar EH.

**Verificación:** el IR del corpus queda **byte a byte idéntico** después del arreglo. Eso no es
casualidad ni suerte — es la prueba de que el código ahora produce exactamente lo que el shim estaba
falseando. El shim se conserva con un comentario: su rama ya no se dispara, y queda como **canario**
por si alguien vuelve a crear el atributo con 0.

## A.2 El evaluador de constantes: una tabla, y un guardián que no existía

La tabla de anchos y signos de los tipos enteros estaba escrita **dos veces**, en
`ProjectIntegerEvaluator.cpp` y en `ProjectTypeConversion.cpp`, con un comentario que decía:

> *"DICE LO MISMO que `ProjectIntegerEvaluator::kIntegerTypes`, y está escrita dos veces A
> PROPÓSITO: es el evaluador de constantes quien llama aquí, no al revés... Las dos tablas se fijan
> la una contra la otra en `nsc.cc1.integer-evaluator` y en la fixture c17."*

**La mitad de eso era cierta y la otra no.**

Cierta la dirección de dependencia: `ProjectIntegerEvaluator` llama a `ProjectTypeConversion`, así
que éste no puede incluir al primero.

**Falsa la prueba.** `nsc.cc1.integer-evaluator` comprueba las respuestas del evaluador
(`ProjectIntegerEvaluator::isSignedInteger("builtin:int")` y compañía) y **no toca la tabla de
`ProjectTypeConversion` en ninguna línea**. No había comprobación cruzada. Las dos copias coincidían
por vigilancia, no porque algo lo impidiera. Es la sexta vez esta semana: un comentario que promete
un guardián que no está (docs 102, 103, 105 y 106).

**Arreglo:** `include/nativesharp/Frontend/BuiltinIntegerTypes.h`. Una cabecera de datos `constexpr`
no tiene dirección de dependencia, así que respeta el motivo real y elimina la copia. Lleva cuatro
`static_assert` que fijan en el **build** —no en una fixture tres pasos más abajo— que `char` mide 8
bits sin signo (CC1-100), que `sbyte` es el único de 8 con signo (ID 89), que `nint` mide 64, y que
`bool` **no** es un entero incorporado.

**Verificación:** IR del corpus byte a byte idéntico.

## A.3 Los tres caminos del HIR pasan a ser uno, y el peor de los tres desaparece

El doc 106 fusionó dos de las tres listas de operaciones núcleo. Faltaba la tercera, y era **la peor
de las tres**:

```cpp
ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::StringConcat;   // <- la trampa
if (core->kind == ...::StringEquals) kind = ...::StringEquals;
else if (...)   // 49 ramas
```

Olvidar una rama no dejaba la operación sin nodo: **la convertía en `StringConcat`**. Es decir,
`File.GetSize(ruta)` se habría bajado como una **concatenación de cadenas**. Ningún mensaje, ningún
aviso; código generado que hace otra cosa.

Ahora `stringCallLowering(kind)` devuelve `{operandCount, hirKind}` — las **tres** preguntas (entra,
con cuántos operandos, en qué nodo) salen del mismo `case`, así que no pueden discrepar. `switch`
sobre los 104 enumeradores, sin `default`.

**Comprobado antes de fusionar, para no fusionar dos cosas que ya discreparan:** las 50 operaciones
autorizadas y las 49 mapeadas explícitamente —más `StringConcat`, que era el valor por omisión— eran
exactamente el mismo conjunto. Cero código muerto, cero operación sin mapeo. La tabla se derivó de
las dos cadenas con un script, no a mano.

**Verificación:** IR byte a byte idéntico. Y la prueba negativa: borrando un `case`,

```
error: enumeration value 'FileGetSize' not handled in switch [-Werror,-Wswitch]
```

## A.4 Diagnósticos: el nodo más interno que falló, con una edición

`ProjectOopHirModel.cpp` tiene **125 sitios** que hacen `error = "..."; return nullptr;` y ninguno
decía en qué parte del programa del usuario estaba el problema.

Adjuntar la ubicación en cada uno serían 125 ediciones, cada una necesitando el nodo correcto a
mano. En vez de eso: `lowerExpression` pasa a ser un **envoltorio** de `lowerExpressionImpl` que, si
el trabajo real vuelve en fallo y todavía no hay rango apuntado, apunta el del nodo que estaba
bajando. Como el fallo se propaga de dentro hacia fuera y sólo se apunta si no hay nada apuntado,
gana el nodo **más interno** — el que de verdad no se pudo bajar.

`lastErrorRange()` lo expone. Traducirlo a `fichero:línea:columna` necesita el `SourceManager`, que
este modelo no tiene; quien reporta el error sí.

**Lo que NO hice, y lo digo:** llevar el `SourceManager` hasta los tres modelos (HIR, MIR, lowerer de
LLVM) para formatear `fichero:línea:columna` en el mensaje. Es plumbing por los constructores y las
llamadas de tres clases, el pago es calidad de diagnóstico y no corrección, y **no se puede verificar
con la equivalencia de IR** — los caminos de error no los ejercita ninguno de los 175 proyectos
verdes, sólo los 5 negativos intencionados. Hacerlo mal sería peor que no hacerlo. Queda **anotado,
no hecho**, con la mitad difícil ya resuelta: el rango ya se recuerda.

## A.5 La fixture de los contratos: `c30_contratos`

Los docs 110 y 111 escriben lo que el lenguaje promete. `c30` comprueba que el compilador lo
**cumple**, y sobre todo que lo cumple **igual en -O0 y en -O2** — una divergencia sale como un rojo
en una sola de las dos configuraciones, que es la forma en que se detecta.

Ocho pasos: wrap con signo, wrap sin signo, promoción antes del desbordamiento, conversiones que
truncan, `MinValue / -1`, **plegado contra ejecución**, coma flotante sin fast-math, y
desplazamientos.

**El paso 6 es el que justifica la fixture** y el que ninguna otra prueba del árbol hacía: la misma
expresión evaluada por `ProjectIntegerEvaluator` en compilación y por el binario en ejecución, ocho
veces. Si discrepan, hay dos juegos de reglas — el fallo que la unificación de A.2 existe para
impedir.

Los valores de entrada **no son literales donde importa**: arrancan de
`Environment.GetCommandLineArgCount()`, que el compilador no puede conocer, así que no puede plegar
lo que la fixture quiere que ejecute.

### Lo que la inspección del IR ya dijo

Dos de las seis mediciones del doc 110 son de inspección y **salieron a favor del contrato**:

- **`nsw`/`nuw`: no se emiten.** 313 `add i64`, 207 `add i32`, 114 `sub i32`, 239 `mul i64`… todas
  desnudas. El compilador no afirma "esto no desborda", así que el optimizador no puede deducir nada
  del desbordamiento.
- **Banderas rápidas de coma flotante: ninguna.** Ni `fast`, ni `reassoc`, ni `contract`, ni `nnan`,
  ni `ninf`. La garantía de once cifras de `String.ToString(double)` (doc 92) no está en riesgo por
  esta vía.

**Clasificación, que es lo que M1-A pide:** las dos son **comportamiento permitido y ya correcto**.
No había bug en la aritmética emitida. Lo que faltaba no era el comportamiento: era el contrato
escrito y una prueba que lo fije.

## A.6 Un hueco del lenguaje, encontrado al escribir la fixture

**`-2147483648` no se puede escribir como literal `int`.**

```
cannot initialize local 'plegadoResta' of type 'builtin:int' from 'builtin:long'
```

El menos unario se aplica a `2147483648`, que no cabe en `int` y por tanto es `long`. C# trata el par
`-` + literal como caso especial; NativeSharp no. Se rodea con `-2147483647 - 1`. Anotado con
condición de reapertura, no arreglado.

---

# Parte C — el primer programa real

`examples/project_demo_ventana/` abre una ventana Win32, lee un asset, pinta en un bucle y
**termina**.

## C.1 Por qué existe

El plan nuevo exige (§3, regla 1) un **consumidor medido** para cada característica, y el plan no
tenía ninguno: no existía ni un juego ni una herramienta escrita en NativeSharp. Sin eso, el orden
de M2/M4/M6 sale del gusto y no de la medición. Este programa es el consumidor mínimo, y ya ha
producido dos hallazgos que ninguna fixture de biblioteca habría encontrado.

## C.2 Qué hace, y qué no necesitó del compilador

**Cero cambios en el compilador.** `extern C from "user32.dll" { ... }` es una característica del
lenguaje que ya existía (Interop Foundation), y las bibliotecas de importación se generan solas para
los imports que declara el **usuario** — es la primera vez que se comprueba, porque hasta hoy los
únicos imports del proyecto eran del runtime.

Catorce funciones, de dos DLLs nuevas para el proyecto:

```
user32.dll   CreateWindowExA DestroyWindow PeekMessageA TranslateMessage
             DispatchMessageA GetDC ReleaseDC FillRect SetWindowTextA
gdi32.dll    CreateSolidBrush DeleteObject TextOutA SetTextColor SetBkColor
```

Y structs por puntero cruzando la frontera: `MSG` (48 bytes) y `RECT`, declarados en NativeSharp y
pasados con `&local`. Más una llamada de **doce argumentos** (`CreateWindowExA`).

**Usa la clase predefinida `"STATIC"` del sistema a propósito.** Registrar una clase propia exige un
`WNDCLASSEXA` con un puntero a función en un campo — otra pieza, y no la que este programa está
midiendo. Se añadirá cuando el programa necesite manejar mensajes de verdad.

Y usa la biblioteca de los Bloques 4 y 5 para el asset: `StringBuilder` para armar el texto,
`File.WriteAllText`/`ReadAllText` para el viaje a disco, `Number.TryParseUInt32` para leer el número
de cuadros del propio fichero.

## C.3 Los dos hallazgos

**1. No hay conversión de `string` a cadena C con NUL para código de usuario.**

Un `string` de NativeSharp es `{i64 longitud; datos}` y **no promete un NUL al final**; Win32 lo
exige. Las operaciones núcleo del runtime (`File.Exists` y compañía) lo resuelven por dentro, pero
eso no está expuesto, así que **cualquier** programa que llame a una API de Windows por `extern C`
tiene que escribir esto a mano:

```csharp
private static byte* ComoCadenaC(string texto, byte[] destino)
```

Es un consumidor medido de una función que la biblioteca debería tener, y encaja exactamente en
M2.3 del plan (Unicode de plataforma) o en M6.1. **Sin este programa no habría aparecido**: ninguna
fixture de biblioteca llama a Win32 desde NativeSharp.

**2. Dos diagnósticos de ownership que un programa normal encuentra en la primera hora.**

- `raii string x = "literal";` → *"RAII ownership cannot be copied or borrowed"*. Un literal no es
  una llamada que devuelva `owned` (ya estaba en el doc 105, y volvió a pasar).
- `Number.TryParseUInt32(String.Substring(...), out v)` → *"ownership-transfer argument requires an
  owned parameter"*. Hay que ligar el `Substring` a un `raii` antes de pasarlo.

Los dos son correctos. Los dos aparecen en cuanto alguien escribe código de verdad, y ninguno dice
la línea — que es exactamente el problema que A.4 empieza a resolver.

## C.4 Entra como BUILD-ONLY, y por qué

`expected = None` en las dos tablas de casos, como `c1_13_two_libraries_same_symbol`.

- **Lo que hay que medir en cada corrida es que ENLAZA** contra `user32.dll` y `gdi32.dll`, y eso lo
  prueba el enlace. Es la primera vez que el proyecto importa de esas dos DLLs y la primera vez que
  se comprueba que las bibliotecas de importación se generan bien para imports declarados por el
  usuario.
- Ejecutarlo abriría **dos ventanas** (una por nivel de optimización) en medio de una corrida de 100
  trabajos en paralelo. Se ejecuta a mano, que para eso es un demo.

---

## Verificación de toda la entrega

```
./sweep.sh llvm  ->  OK=175  FAIL=5      (eran 173; +c30 +demo_ventana; los 5 negativos de siempre)
```

**Los tres refactores del compilador, con IR byte a byte idéntico** en los 149 proyectos comunes,
comparado contra el compilador anterior en cada paso — no contra un modelo mío:

| Cambio | sha256 del corpus antes | después |
|---|---|---|
| ID 130 | (shim ya forzaba 2) | idéntico |
| `BuiltinIntegerTypes.h` | `790995f5…4b30711` | `790995f5…4b30711` |
| tabla del HIR fusionada | `790995f5…4b30711` | `790995f5…4b30711` |
| envoltorio de `lowerExpression` | `790995f5…4b30711` | `790995f5…4b30711` |

Prueba negativa del guardián del HIR: borrando un `case`, error de compilación que **nombra el
enumerador**.

Las dos tablas de casos comprobadas idénticas caso por caso: **50 y 50**.

### Lo que no puedo verificar desde el contenedor

- Que el arreglo de ID 130 cambie algo en Windows con LLVM 23 (aquí el shim lo tapaba).
- Los pasos 1–8 de `c30` ejecutando: wrap, `MinValue / -1`, y **el plegado contra la ejecución**.
- Que `demo_ventana` **enlace** contra user32/gdi32 — el frontend llega al IR y para. Esa es
  precisamente la medición que el corpus va a hacer por primera vez.
- Que la ventana se vea. Eso lo ejecutas tú.

## Números esperados en la próxima corrida

```
ctest      244   (c30 va DENTRO de nsc.cc1.correctness)
trabajos   100   (50 casos x 2, eran 96; uno de ellos build-only)
triage   16/16
```
