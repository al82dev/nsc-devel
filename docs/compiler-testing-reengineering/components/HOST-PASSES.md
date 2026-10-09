# Host.Passes — Optimización MIR SSA y administración de passes

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 42 archivos, 3,956 líneas y 113,411 bytes.
- **Inventario:** 158 símbolos físicos —25 tipos y 133 funciones/helpers—, 56 contratos y 448 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado contractual y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.Passes recibe el MIR en forma SSA producido por `Host.SSA`, crea una copia mutable independiente y ejecuta
una tubería iterativa de optimizaciones. Su frontera completa es:

```text
SsaModule verificado
  -> OptimizationBuilder
  -> OptimizationModule mutable
  -> AnalysisManager (CFG y dominancia bajo demanda)
  -> PassManager
       PhiSimplification
       CopyPropagation
       ConstantPropagation
       DeadCodeElimination
       SimplifyControlFlow
  -> verificación después de cada mutación
  -> punto fijo
  -> CFG y dominancia finales verificadas
```

No construye HIR, MIR inicial ni SSA. No baja a LLVM. No enlaza. Sus mutaciones sólo afectan la copia MIR que
posee `OptimizationArena`; el `SsaModule` fuente permanece como procedencia no propietaria.

## 3. Contratos base de la tubería

### 3.1 Análisis requeridos y preservados

`AnalysisKind` contiene exactamente `Cfg` y `Dominance`. `PassRequirements` forma la tabla:

| Fábrica | CFG | Dominancia |
|---|---:|---:|
| `none()` | no | no |
| `cfg()` | sí | no |
| `dominance()` | sí | sí |

`PreservedAnalyses` ofrece `all()`, `none()` y `cfgAndDominance()`. En la implementación vigente, `all()` y
`cfgAndDominance()` son equivalentes porque sólo existen esos dos análisis. `preserves` devuelve false para un
valor fuera del dominio conocido.

### 3.2 Protocolo de un pass

`MirPass` exige cuatro operaciones: nombre, requisitos, análisis preservados y `run`. `PassRunResult` informa si
hubo cambio y cuántas transformaciones se realizaron. `PassContext` conserva exactamente el módulo MIR mutable,
su arena y el `AnalysisManager`; no crea sustitutos ni toma ownership.

`PassRecord` registra por pass e iteración:

- nombre y número de iteración;
- cambio y transformaciones;
- instrucciones y bloques antes/después;
- preservación de CFG/dominancia;
- acumulados de reconstrucciones de CFG/dominancia después de verificar el resultado.

## 4. Ownership y estado persistente

`OptimizationArena` posee tres arenas —MIR, CFG y Dominance— y todos los `OptimizationModule`. Al destruirse,
elimina primero los módulos; las arenas miembro sobreviven durante esa destrucción y luego liberan sus objetos.

`OptimizationModule` conserva:

- puntero no propietario al `SsaModule` fuente;
- puntero al MIR clonado y a la arena que lo posee;
- punteros sustituibles al CFG y dominancia vigentes;
- registros ordenados de passes;
- iteraciones completadas;
- conteos finales de construcciones de CFG y dominancia.

Los setters de análisis permiten null porque la invalidación es una transición válida. La validez de los punteros
no se deduce por presencia: `OptimizationVerifier` vuelve a comprobar los análisis antes de aceptar el módulo.

## 5. Clonación SSA → MIR optimizable

### 5.1 Entrada y ciclo de vida

`OptimizationBuilder::build` limpia error y mapas en cada llamada. Rechaza fuente nula o sin MIR. Crea un nuevo
`MirModule`, clona el grafo y sólo entonces crea el `OptimizationModule`.

### 5.2 Globales

Cada global exige objeto y símbolo. Se crea un global nuevo con la misma identidad de símbolo; si la fuente tiene
inicializador explícito, se copia el valor y su marca. La secuencia se conserva.

### 5.3 Funciones, bloques, valores y parámetros

Por función se limpian mapas de bloques y valores. Se crea una función nueva con el mismo símbolo y estado
`defined`. Un prototipo termina ahí. Para una definición:

1. se copian las identidades de locales;
2. se crean todos los bloques en orden y se indexan por identidad fuente;
3. se precrea un valor nuevo por cada resultado de instrucción;
4. se traducen los parámetros mediante ese mapa;
5. se clonan las instrucciones en orden.

Precrear valores permite referencias hacia definiciones que todavía no se han clonado físicamente. Un parámetro
sin resultado SSA correspondiente, un bloque nulo o una instrucción nula abortan la operación.

### 5.4 Instrucciones

La clonación conserva opcode, rango, resultado, operandos, pares PHI, símbolo, tipo auxiliar, constante,
conversión, inmediato, offset y hasta dos targets. Resultados, operandos, valores PHI y targets deben estar en sus
mapas; ninguna referencia se conserva hacia bloques o valores del MIR fuente.

## 6. Plegado de constantes

### 6.1 Modelo entero

Los anchos vigentes son 8, 16, 32 y 64 bits. `Short`, `Int`, `Long`, `NInt` y `Enum` son firmados; `Byte`,
`Char`, `UShort`, `UInt`, `ULong` y `NUInt` son no firmados. Toda operación se normaliza mediante máscaras. La
conversión de bits a firmado implementa complemento a dos explícitamente; para 64 bits usa `bit_cast`.

### 6.2 Conversiones

- entero → entero conserva los bits bajos del origen y los interpreta según el destino;
- entero → floating convierte numéricamente y redondea vía `float` si el destino es `Float`;
- floating → floating conserva `double` o redondea vía `float`;
- floating → entero trunca hacia cero y sólo pliega valores finitos dentro del rango del destino.

Un tipo nulo, combinación no soportada, NaN/infinito o resultado fuera de rango produce `nullopt`; no inventa una
semántica de excepción.

### 6.3 Operadores

`foldUnary` cubre not lógico booleano, negación floating/entera y not bitwise. `foldComparison` separa booleanos,
floating, enteros firmados y no firmados. `foldBinary` cubre aritmética, bitwise y shifts.

La aritmética entera envuelve al ancho. División/resto no se pliegan ante cero ni ante `min / -1`. Los shifts
requieren `0 <= count < width`; el shift derecho firmado extiende signo mediante máscara explícita.

### 6.4 Dispatch y igualdad

`ConstantFolder::fold` reconoce:

- constantes existentes;
- copias cuyo operando es conocido;
- PHIs no vacíos cuyos incoming son todos conocidos e iguales;
- conversiones, unarios, comparaciones y binarios con aridad exacta.

La igualdad primero exige el mismo `ConstantKind`. Floating compara el patrón de 64 bits, por lo que distingue
ceros con signo y cargas NaN distintas. `None` y `Null` son iguales dentro de su propio kind.

## 7. Efectos y reescritura MIR

`MirEffectClassifier` produce cinco banderas: efecto lateral, trap, lectura, escritura y estructura. Operaciones
aritméticas no divisorias, comparaciones, direcciones y extracción son puras. Divide, remainder, shifts y bounds
check pueden atrapar. Una conversión sólo puede atrapar en la ruta floating → entero. Loads leen; stores escriben
y tienen efecto. Calls y allocations tienen efecto; array/stack allocation además pueden atrapar. Terminadores y
parámetros son estructurales.

Una instrucción sólo es eliminable cuando tiene resultado y ninguna bandera está activa. Esta conjunción evita
que DCE retire operaciones observables o potencialmente excepcionales.

`MirRewriter::replaceAllUses` recorre funciones definidas y reemplaza usos ordinarios y PHI, contabilizando ambos
antes de llamar al mutador de la instrucción. No cambia definiciones. Sus contadores de instrucciones y bloques
ignoran prototipos y devuelven cero ante módulo nulo.

## 8. Passes transformadores

### 8.1 Simplificación PHI

Primero indexa definiciones de la función. Sólo examina el prefijo PHI de cada bloque:

- incoming idénticos se convierten en `Copy`;
- incoming definidos por constantes bit-exactamente iguales se convierten en `Constant`;
- los demás PHI permanecen.

El bloque resultante ordena PHIs retenidos, reemplazos y cuerpo. Un PHI fuera del prefijo no se toca. El resultado
y rango originales se conservan en la instrucción nueva.

### 8.2 Propagación de copias

Se indexan copias válidas de un operando con resultado distinto. `resolveReplacement` sigue cadenas de forma
iterativa y usa un set visitado para cortar ciclos. Después sustituye todos los usos por el terminal y elimina las
copias indexadas. El pass preserva CFG y dominancia porque no cambia bloques ni edges.

### 8.3 Propagación de constantes

Se inicializa un mapa con constantes explícitas y se recorre el módulo hasta que ninguna instrucción produzca un
nuevo valor conocido. En la segunda pasada se materializan como `Constant` los resultados plegables, excepto
opcodes ya constantes, parámetros, undef, PHI y copy. La exclusión evita solapar la responsabilidad de los passes
PHI/copy y conserva la forma esperada del pipeline.

### 8.4 Eliminación de código muerto

Por función se recalculan usos ordinarios y PHI. Se elimina una definición sin usos sólo si el clasificador la
declara removible. Como retirar una instrucción puede dejar sin usos a sus operandos, repite el proceso hasta punto
fijo. El pass no depende de recursión.

### 8.5 Simplificación de control de flujo

Este pass requiere CFG y no preserva análisis. Convierte `Branch` a `Jump` cuando ambos targets son iguales o la
condición está definida por una constante booleana. Sólo si una función tuvo esa transformación:

1. calcula reachability desde entry mediante stack explícita;
2. elimina bloques no alcanzables;
3. reconstruye predecessors de los bloques retenidos;
4. elimina incoming PHI ajenos o duplicados.

La implementación vigente no realiza una limpieza general de inalcanzables si no simplificó al menos una rama.
Perder entry o no poder derivar predecessors es un error del pass.

## 9. Administración de análisis

`AnalysisManager` construye CFG bajo demanda, lo verifica y sólo después incrementa el contador y lo publica en
el módulo. Dominancia primero exige CFG, luego construye y verifica su módulo. Una segunda solicitud devuelve el
puntero cacheado sin reconstruir.

La invalidación sigue estas transiciones:

| Preservación | CFG | Dominancia |
|---|---|---|
| no preserva CFG | se borra | se borra |
| preserva CFG, no dominancia | permanece | se borra |
| preserva ambos | permanece | permanece |

La misma transición se refleja tanto en el manager como en `OptimizationModule`, evitando que el verificador
consuma un análisis declarado obsoleto.

## 10. Máquina de estados del PassManager

El manager posee los passes agregados y los destruye. Ignora punteros null en `addPass`. El pipeline por defecto,
en orden, es:

```text
phi-simplification
copy-propagation
constant-propagation
dead-code-elimination
simplify-control-flow
```

`setMaximumIterations(0)` normaliza a uno; el valor inicial es ocho. `run`:

1. limpia el error y valida módulo, arena y pipeline;
2. crea el administrador de análisis;
3. verifica el estado inicial;
4. por iteración y pass, satisface requisitos;
5. captura conteos previos;
6. ejecuta el pass;
7. ante cambio, invalida lo no preservado y verifica inmediatamente;
8. captura conteos posteriores y agrega `PassRecord`;
9. termina tras una iteración completa sin cambios;
10. rechaza falta de convergencia y realiza verificación final.

Los registros anteriores del módulo no se limpian al iniciar una nueva ejecución; el contrato vigente es append.
Un error de pass se distingue de un resultado inválido, un fallo de análisis o falta de convergencia.

## 11. Verificación de la salida optimizada

Existen dos entradas: una con `AnalysisManager`, que asegura análisis, y otra const, que exige CFG/dominancia ya
cacheados. Ambas convergen en `verifyWithAnalyses`:

1. verifica MIR en modo SSA;
2. verifica CFG;
3. verifica dominancia;
4. exige análisis para cada función definida;
5. rechaza nodos CFG no alcanzables;
6. comprueba definiciones y usos.

Por función se indexa cada resultado con bloque e índice de instrucción y se rechazan definiciones múltiples. Un
uso ordinario exige definición previa en el mismo bloque o dominancia del bloque de uso.

Un PHI exige exactamente un incoming por predecessor, bloques únicos, pertenencia al CFG y que cada bloque
incoming sea predecessor real. El valor incoming debe tener definición y ésta debe dominar el bloque predecessor,
es decir, la arista por la que entra el valor. La consulta usa el árbol de dominadores verificado.

Cada llamada pública limpia error y mapa de definiciones. Los retornos tempranos conservan el diagnóstico de la
primera violación observada en ese recorrido.

## 12. Dumps reproducibles

`OptimizationDumper` emite `optimized.ssa.module`, globales, funciones, bloques e instrucciones en el orden
almacenado. Publica símbolos, tipos auxiliares, constantes, conversiones, inmediatos, offsets, operandos, incoming
PHI y targets. Valores y bloques usan IDs MIR.

Strings escapan backslash, comillas, controles conocidos y bytes fuera de ASCII imprimible como `\xNN`, restaurando
flags y fill usados por el escape hexadecimal. Floating usa precisión 17 y deja esa precisión configurada en el
stream; es parte del efecto observable vigente. Un módulo nulo no escribe nada.

`PassPipelineDumper` publica iteraciones, conteos de análisis y una línea exacta por `PassRecord`, incluido el
estado de preservación. Un módulo nulo tampoco escribe.

## 13. Complejidad y límites vigentes

- Clonar el módulo es lineal en globales, funciones, bloques, instrucciones y edges de operandos/PHI.
- El plegado individual es constante salvo PHI, que es lineal en sus incoming.
- Propagación de constantes itera hasta cierre y vuelve a recorrer todo el módulo por ronda.
- Propagación de copias sustituye cada mapeo recorriendo todo el módulo; su costo puede ser cuadrático en copias y
  usos.
- DCE recalcula usos después de cada ronda; cadenas largas pueden exigir tantas rondas como definiciones.
- Reachability y predecessors usan memoria explícita proporcional a bloques y edges; no hay recursión.
- El pipeline completo está acotado por `maximumIterations` y falla si no alcanza punto fijo.
- El verificador usa mapas/sets de definiciones, predecessors y análisis; privilegia independencia y diagnóstico
  sobre costo mínimo.

Estos límites son hechos estáticos de la revisión congelada. No autorizan optimización ni refactor en esta fase.

## 14. Estrategia de prueba futura

Los 56 contratos aparecen en `test-plans/HOST-PASSES-TEST-PLAN.md`, con ocho matrices por contrato: 448 casos,
392 `PR` y 56 `NIGHTLY`. Los oracles no reutilizarán el algoritmo de producción:

- aritmética de referencia por bits y anchos explícitos;
- cierre de constantes y liveness calculados por fixtures pequeños;
- reachability y predecessors independientes;
- relaciones uso-definición y dominancia esperadas construidas en el fixture;
- passes falsos para observar la máquina de estados del manager;
- comparación byte a byte de dumps.

Los casos de escala deben cubrir cadenas de copias, propagación y DCE largas, grafos profundos y pipelines que
convergen justo en el límite o no convergen. Ningún caso contabiliza `if` internos como contrato manual.

## 15. Reconstructibilidad

El inventario de 42 archivos, 158 símbolos, 56 contratos y 448 casos permite reconstruir ownership, clonación,
plegado, efectos, reescritura, los cinco passes, caché e invalidación de análisis, administración hasta punto fijo,
verificación SSA optimizada y ambos dumps. La segunda pasada está en
`audits/TANDA-07-HOST-PASSES-AUDIT.md`.

**Resultado:** `RECONSTRUIBLE` para la revisión congelada, sin afirmar ejecución ni cobertura.
