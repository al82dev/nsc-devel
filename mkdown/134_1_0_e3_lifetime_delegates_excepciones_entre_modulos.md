# 134 — 1.0-E3: lifetime de delegates, closures y excepciones entre módulos

## Resultado de la entrega

E3 quedó implementado sobre la frontera binaria real. Un productor NativeSharp puede devolver un
delegate ordinario —a método estático, método de instancia o lambda con capturas— desde una `.lib`
o una DLL; el consumidor lo invoca y, cuando lo declara `raii`, destruye exactamente la memoria que
recibió mediante funciones pertenecientes al productor.

La misma matriz prueba excepciones tipadas lanzadas dentro del productor y capturadas en el
consumidor. El objeto, su identidad RTTI y los `finally` sobreviven el salto de módulo en O0 y O2.

No se cambió la representación de `extern C delegate`: continúa siendo un puntero de función crudo.
La celda que se describe aquí pertenece únicamente al delegate ordinario de NativeSharp.

## El estado que había antes

Stage 42 y Stage 43 ya tenían casi toda la semántica dentro de una compilación:

- conversión de grupos de métodos estáticos y de instancia;
- lambdas sin capturas y closures con snapshots de capturas;
- invocación mediante código más receptor oculto;
- igualdad por el par código/receptor;
- rechazo de capturas prestadas o de owners cuyo lifetime no puede demostrarse.

Pero la representación era un detalle privado de 16 bytes:

```text
+0  código
+8  receptor o entorno
```

Eso dejaba dos defectos concretos. NSMETA reconstruía exclusivamente delegates `extern C`, por lo
que un delegate ordinario escrito en una firma pública desaparecía al consumir una biblioteca. Y
la celda no decía quién debía liberar el entorno de una closure; cruzar una DLL habría obligado al
consumidor a adivinar el allocator del productor.

Las excepciones tenían otro problema del mismo tipo. `RaiseException` ya transportaba el objeto en
`ExceptionInformation[0]`, pero cada módulo tenía su propio índice TLS. El filtro del consumidor
aceptaba el código SEH y luego preguntaba por el objeto en su TLS local, que nunca había recibido el
valor guardado por la DLL. El resultado observable era la excepción privada `0xE04E5301` escapando
sin que el `catch` del ejecutable pudiera materializar su objeto.

## La celda ABI de delegate ordinario

Delegate ABI 1 congela una celda de 32 bytes en Windows x64:

| Offset | Campo | Propietario y uso |
|---:|---|---|
| 0 | `code` | dirección del método o callable sintético |
| 8 | `context` | receptor prestado o entorno de closure |
| 16 | `destroyContext` | callback opcional del productor |
| 24 | `freeCell` | callback del productor que libera la celda |

La distinción entre receptor y entorno es deliberada:

- un método de instancia presta el receptor; destruir el delegate no destruye el objeto;
- una closure posee su entorno; `destroyContext` lo devuelve al allocator que lo creó;
- toda celda posee su propia reserva; `freeCell` evita liberar memoria de una DLL desde una
  implementación de runtime distinta;
- una lambda sin capturas tiene contexto nulo, pero su celda sigue siendo liberable.

No hay conteo de referencias oculto. Una copia de un delegate no-owner copia la referencia, como
ya ocurría. Cuando existe propiedad, NativeSharp aplica sus reglas normales: un owner no se copia;
se transfiere con `move`, y un `raii` ejecuta una sola descarga. Así se conserva una superficie
familiar sin introducir una operación de retención invisible en una ruta sensible.

## Cómo entra una closure a ownership

`bindLambdaToDelegate` y `bindMethodGroupToDelegate` marcan la creación como
`ownershipFreshOwner`. Esta tercera categoría ya existía para `new T(...)`: el valor acaba de ser
reservado y todavía no procede de una variable que pueda marcarse como movida.

Eso permite las formas siguientes:

```csharp
raii Transform callback = value => value + bias;
public static owned Transform Make() { return value => value + 2; }
```

La forma histórica sin calificador sigue compilando. No se convirtió cada delegate existente en un
owner por sorpresa. Quien quiera cleanup determinista lo expresa con `raii` o con el contrato
`owned` de parámetro/retorno.

`typeKeyIsOwnable` reconoce únicamente el delegate ordinario. Un `extern C delegate` no posee una
celda ni un entorno y por ello nunca entra a esta ruta de destrucción.

## Destrucción normal y excepcional

`requiresOwnedDestruction` incluye ahora delegates ordinarios. Los tres puntos que descargan
ownership usan la misma operación `emitDelegateDestroy`:

1. cleanup de una local `raii`;
2. cleanup de un parámetro `owned`;
3. `delete` explícito.

La operación primero anula el slot owner mediante la guarda de cleanup existente. Después:

1. comprueba si existe contexto y destructor de contexto;
2. llama ese callback con el entorno;
3. carga `freeCell` de la propia celda;
4. libera la celda;
5. continúa al bloque de salida.

La anulación previa del owner es lo que conserva la garantía frente a una entrada duplicada del
cleanup por unwind: aunque Windows visite la región y luego exista una salida estructurada, la
segunda entrada ve nulo y no vuelve a destruir.

## Excepciones a través de DLL

La corrección vive en `buildEhFilter`, no en cada `catch`. Después de comprobar el código SEH, el
filtro valida que exista al menos un parámetro y lee el puntero de objeto desde el layout Win64 de
`EXCEPTION_RECORD`:

```text
offset 24  NumberParameters
offset 32  ExceptionInformation[0]
```

El filtro inicializa el TLS del módulo que está manejando la excepción y publica ahí el objeto.
Desde ese momento el camino existente —`__nativesharp_exception_current`, filtro RTTI, catchpad,
`finally` y `__nativesharp_exception_clear`— funciona igual dentro de un ejecutable y entre DLL.

Esta arquitectura no necesita una global compartida ni un runtime singleton. Windows ya es el
transporte entre módulos; el filtro sólo reconstruye el estado local a partir del registro recibido.

## Materialización desde NSMETA

`canMaterializeContractType` acepta ahora dos contratos distintos:

- un callback crudo requiere la firma introducida en NSMETA 1.5;
- un delegate ordinario requiere NSMETA 1.8, Delegate ABI 1 y celda de 32 bytes.

`buildPrecompiledPublicSource` conserva la distinción. Sólo ante el bit `extern C` escribe esas
palabras; en los demás casos reconstruye `public delegate ...`.

También se cerró una dependencia descubierta por la fixture de excepciones: materializar una clase
pública recorre su base y sus interfaces, y el archivo sintético publica los `using` necesarios. Un
tipo de excepción ya no llega al consumidor sin la cadena que determina su identidad RTTI.

## Archivos y funciones modificados

### Frontend

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - `analyzeVariable`: admite delegate ordinario como owner `raii`/`owned`.
  - `analyzeReturn`: reconoce una celda recién creada como retorno `owned` válido.
  - `bindLambdaToDelegate`: marca la closure como owner fresco.
  - `bindMethodGroupToDelegate`: hace lo mismo sólo para delegate ordinario.
  - `typeKeyIsOwnable`: centraliza la nueva categoría.

### Backend LLVM

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - `allocateDelegateCell`: pasa de 16 a 32 bytes y escribe los dos callbacks.
  - `emitDelegateDestroy`: nueva descarga común, nula-segura y dirigida por el productor.
  - `lowerLambdaCreate`: instala destructor de entorno cuando hay capturas.
  - `lowerDelegateCreate`: crea una celda con receptor prestado.
  - `emitRaiiDestroy`, `emitOwnedParameterDestroy` y `lowerDeallocation`: descargan delegates.
  - `requiresOwnedDestruction`: comparte la clasificación con los tres caminos.

### Runtime

- `src/Runtime/WindowsRuntimeBuilder.cpp`
  - `buildEhFilter`: importa el objeto desde `EXCEPTION_RECORD` al TLS del módulo manejador.

### Bibliotecas precompiladas

- `src/Driver/Driver.cpp`
  - `canMaterializeContractType`: valida Delegate ABI 1 para delegates ordinarios.
  - `collectMaterializedContractTypes`: recorre bases e interfaces.
  - `buildPrecompiledPublicSource`: conserva delegate ordinario frente a `extern C delegate` y
    emite namespaces requeridos por la jerarquía.

### Pruebas

- `tests/unit/Stage43LambdasClosuresTests.cpp`
  - certifica la celda de 32 bytes;
  - certifica `raii delegate` y los callbacks de entorno/celda en LLVM.
- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - productor y consumidor separados para `.lib` y DLL;
  - método estático, closure capturada y método de instancia;
  - excepción tipada y `finally` cruzando ambos productos;
  - ejecución O0 y O2 con resultado 42.

## Evidencia de cierre

La fixture E3 ejecuta ocho combinaciones principales:

| Producto | Caso | O0 | O2 |
|---|---|---:|---:|
| `.lib` | delegates/closure/instancia | 42 | 42 |
| `.lib` | excepción tipada + `finally` | 42 | 42 |
| DLL | delegates/closure/instancia | 42 | 42 |
| DLL | excepción tipada + `finally` | 42 | 42 |

Además, el módulo LLVM del owner verifica antes y después de optimización. La prueba del runtime
confirma que el filtro conserva el código privado, TLS, `RaiseException` y el payload nativo.

## Contrato que queda para 1.0

- El target de un delegate de instancia es prestado: el owner del objeto debe vivir más que el
  delegate. El compilador continúa rechazando capturas cuyo escape no puede justificar.
- Las capturas son snapshots de valores; un owner o préstamo no puede esconderse dentro del
  entorno.
- Un owner de delegate se mueve y se destruye una vez. No existe copia implícita de ownership.
- El detalle físico ya no puede cambiar silenciosamente: E4 lo coloca en el contrato ABI 1.0 y en
  la huella de NSMETA.

## Cierre de validación del bloque E3

La validación completa del 7 de septiembre de 2026 no se limitó a inspeccionar LLVM. El binario
recién construido ejecutó el catálogo completo y obtuvo **297/297**. Dentro de esa corrida quedaron
verdes, entre otras, estas rutas que se solapan directamente con E3:

- `nsc.stage42.delegates-events`;
- `nsc.stage43.lambdas-closures`;
- `nsc.stage45.native-exception-runtime`;
- `nsc.stage45.typed-catch-rtti`;
- `nsc.stage46.native-libraries`;
- `nsc.stage49.raii-move`;
- toda la familia Stage 50 de ownership, préstamos, escape analysis y lowering.

La fixture Stage 46 es la prueba de producto: compila productor y consumidor por separado, enlaza
como biblioteca estática y como DLL, y ejecuta los casos en O0 y O2. El caso DLL reveló el defecto
real del TLS por módulo; antes de la corrección terminaba con el código SEH crudo `0xE04E5301` en
vez de entrar al `catch`. Después de importar `ExceptionInformation[0]` en `buildEhFilter`, las ocho
combinaciones devuelven 42 y el `finally` se ejecuta exactamente una vez.

Durante la corrida completa también se corrigieron expectativas históricas que todavía describían
la celda antigua de 16 bytes y el diagnóstico anterior de `raii`. No se relajaron verificaciones:
ahora certifican explícitamente los cuatro punteros de la celda y reconocen al delegate administrado
como una categoría ownable.

## Certificación instalada

La evidencia local anterior quedó promovida a baseline instalado el 7 de septiembre de 2026. G00
`20260907-112232-d023cfe0` terminó `LANDED`, `certified: true`, con cero fallos. El mismo árbol fue
medido antes y después, y el binario construido coincide byte por byte con el instalado. Por tanto,
la semántica E3 descrita aquí ya no depende sólo del ejecutable de desarrollo: forma parte del
compilador certificado y pasó las rutas de lld-link y NSLink.
