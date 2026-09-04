# 121 — Next Steps for the 1.0 Glory

**Fecha:** 2026-09-04  
**Punto de partida:** Native Sharp 0.7.0-alpha conceptual; compilador identificado todavía como
0.1.0-dev55 / Stage 57.  
**Objetivo:** llevar Native Sharp a 1.0 como lenguaje y toolchain nativo para videojuegos,
herramientas gráficas y software de plataforma con rendimiento predecible.

---

## 1. La promesa de Native Sharp 1.0

Native Sharp 1.0 debe permitir escribir, compilar, enlazar, depurar y distribuir un juego nativo
para Windows sin depender de un runtime administrado y sin tener que abandonar el lenguaje para
resolver sus caminos críticos.

La promesa es concreta:

- binarios nativos;
- cero GC y cero pausas introducidas por un recolector;
- ownership, préstamos, transferencias y destrucción visibles y comprobables;
- layout y ABI conocidos;
- costos de asignación, copia y dispatch razonables y observables;
- interoperabilidad directa con C, Win32 y COM;
- semántica idéntica en O0 y O2 salvo diferencias de rendimiento;
- depuración nativa con fichero, línea, símbolos y stack traces útiles;
- suficiente expresividad para escribir gameplay, render, audio, input, assets, herramientas y
  código de engine sin heredar la complejidad histórica completa de C++.

1.0 no significa «tener muchas features». Significa que lo que el lenguaje promete funciona de
punta a punta, es diagnosticable y puede sostener un proyecto real.

---

## 2. Restricción arquitectónica: lo que destruiría el lenguaje

Native Sharp **no es** y **no será** una plataforma para aplicaciones empresariales o de consumo
general. Esto no es backlog diferido. Es una exclusión permanente.

Quedan fuera del lenguaje, de `NativeSharp.Core` y de los criterios de madurez:

- frameworks web;
- servidores HTTP de propósito general;
- SQL como característica del lenguaje o subsistema oficial;
- ORM;
- conectores de bases de datos empresariales;
- LINQ o un sistema de consultas general;
- reflection dinámica;
- inyección de dependencias;
- contenedores de servicios;
- serializers empresariales;
- UI de formularios de negocio;
- compatibilidad con stacks Java/.NET de aplicaciones corporativas;
- cualquier feature justificada principalmente por CRUD, servicios web o software administrativo.

Si una propuesta necesita una de esas áreas para justificar su existencia, se rechaza. Que pueda
implementarse no es motivo para implementarla.

Una necesidad concreta de un juego puede resolverse mediante una biblioteca externa o un binding
nativo sin convertirla en responsabilidad del lenguaje. La frontera es sencilla: Native Sharp
debe poder **llamar código nativo**; no debe absorber todos los dominios de ese código.

Esta exclusión protege tres propiedades:

1. El runtime permanece pequeño, estático y comprensible.
2. El costo de las abstracciones continúa visible.
3. El diseño se decide por frame time, memoria, ABI y herramientas nativas, no por paridad con C#,
   Java o plataformas empresariales.

Agregar superficie web/SQL/enterprise no acercaría Native Sharp 1.0. Lo desviaría de 1.0.

---

## 3. Punto de partida real

El núcleo ya existe. No se vuelve a planificar:

- clases, structs, interfaces, herencia y dispatch virtual;
- constructores, destructores, propiedades, indexadores y operadores;
- excepciones y cleanup por salida normal o excepcional;
- `owned`, `borrowed`, `raii` y `move`;
- definite assignment;
- genéricos monomorfizados y structs genéricos;
- delegates, lambdas y eventos;
- arrays, fixed arrays, spans, colecciones y `foreach`;
- unions, pack, align, offsets y ABI Win64;
- enums con tipo subyacente y nombres sin reflection;
- FFI C, function pointers, callbacks e importación desde DLL;
- hilos, atomics y primitivas de sincronización;
- UTF-8 explícito;
- emisión de OBJ, EXE, LIB y DLL;
- frontend, HIR, MIR y backend LLVM;
- enlace por `lld-link` y NSLink.

Las correcciones del documento 120 cerraron tres interacciones de madurez:

- receptor struct producido por propiedad o indexador;
- transferencia de ownership hacia constructores;
- igualdad de structs por hojas declaradas, sin padding.

La medición más reciente del arnés aislado es 188 positivos y cinco negativos intencionales en las
etapas de cuerpos e LLVM. Falta certificar esa entrega con el build completo y ejecución Windows.

Native Sharp ya sabe qué lenguaje quiere ser. El trabajo hacia 1.0 consiste en demostrar que puede
mantener esa identidad bajo programas grandes, optimización, errores, interoperabilidad y uso
diario.

---

## 4. Escalera de versiones

### 0.7.x-alpha — cerrar el lenguaje existente

No se agregan subsistemas. Se eliminan contradicciones y límites dentro de features ya aceptadas.

### 0.8.x-alpha — toolchain nativo completo

Un proyecto externo al repositorio puede compilar, enlazar y depurarse sin scripts privados ni
conocimiento de la estructura interna del compilador.

### 0.9.x-beta — congelación y juego piloto

La sintaxis, la semántica, el formato de proyecto y las ABI públicas se congelan. Un juego real se
construye y prueba durante suficiente tiempo para descubrir interacciones que las fixtures no ven.

### 1.0.0 — promesa de compatibilidad

La versión deja de describir progreso interno y pasa a ser un contrato con usuarios: el código
válido no cambia de significado silenciosamente y los formatos versionados no cambian sin una
regla de compatibilidad.

---

## 5. Gate 0.7 — Correctness antes que superficie

### 5.1 Certificar la entrega actual

- reconstruir e instalar el `nsc.exe` exacto que se va a medir;
- ejecutar CTest completo;
- ejecutar las fixtures CC1 en O0 y O2 sobre Windows real;
- confirmar los resultados de c41, c42 y c43;
- correr con `lld-link` y NSLink cuando la ruta probada involucre enlace;
- registrar build id, commit, LLVM, linker y resultado;
- no declarar verde una prueba que usó otro compilador instalado.

### 5.2 Cerrar `NSC-LIMIT-005`: observabilidad del optimizador

El caso de enums estrechos negativos demostró que `LLVMRunPasses` y `opt.exe` pueden divergir. Para
1.0 no puede existir un miscompile cuya única explicación sea «LLVM decidió otra cosa».

Se requiere:

- captura reproducible del IR antes y después de cada pipeline relevante;
- identificación exacta del primer pass que cambia el comportamiento;
- verificación automática del módulo después de optimizar;
- fixture de ejecución O0/O2;
- versión de LLVM registrada en cada resultado;
- diagnóstico interno que incluya función y ubicación cuando el lowering produzca IR inválido;
- una política soportada de versiones LLVM, no «la que haya instalada».

**Cierre:** cero divergencia observable O0/O2 en todo el corpus contractual.

### 5.3 Cerrar `NSC-LIMIT-007`: constraints genéricos mínimos

Sólo entra la superficie necesaria para APIs nativas y matemáticas reutilizables:

```csharp
where T : class
where T : BaseClass
where T : IInterface
where T : class, IInterface
```

No se construye un solver general. Las constraints son comprobaciones estáticas sobre
especializaciones monomorfizadas.

Se requiere:

- AST y parser completos;
- validación después de resolver nombres, herencia e interfaces;
- sustitución y clonación sin perder constraints;
- diagnósticos que nombren parámetro, constraint y tipo recibido;
- pruebas en clases, structs, métodos, delegates y sobrecargas;
- ninguna metadata dinámica ni diccionario de tipos en runtime.

### 5.4 Cerrar `NSC-LIMIT-009`: tipos anidados

Los tipos anidados tienen valor para organizar bindings grandes y representar APIs de plataforma.
No entran por paridad con C#.

Se requiere:

- identidad `Outer+Inner` estable;
- lookup lexical y calificado;
- accesibilidad completa;
- mangling estable;
- soporte en genéricos, herencia, metadata, C headers y diagnósticos;
- pruebas con dos tipos internos del mismo nombre en contenedores diferentes;
- ninguna búsqueda dependiente del orden de archivos.

### 5.5 Consolidar reglas transversales

Los últimos defectos repiten una forma: dos capas hacen la misma pregunta y dan respuestas
distintas. Antes de 0.8 deben existir fuentes únicas para:

- clasificación de ownership de argumentos;
- resolución de receptor valor/tipo;
- addressability de campos, propiedades, indexadores y temporales;
- evaluación de constantes;
- descripción de tipos enteros y enums;
- layout/ABI de agregados;
- selección de operaciones núcleo;
- compatibilidad de conversiones.

No es necesario reescribir el frontend. Sí es necesario impedir una cuarta copia de cada regla.

### 5.6 Cerrar decisiones semánticas que todavía permiten una respuesta silenciosa

Antes de llamar completo al lenguaje existente hay que inventariar todos los límites documentados
y clasificarlos como bloqueo de 1.0, rechazo deliberado o feature futura. Ninguno puede quedar como
«compila, pero nadie sabe quién responde por el resultado».

El caso inmediato es el owner recién creado enviado a un parámetro sin calificador:

```csharp
Consumir(new Recurso()); // ¿quién destruye Recurso si el parámetro no es owned?
```

Hoy esa forma puede ser aceptada aunque el receptor no adopte el objeto. Para 1.0 debe rechazarse o
tener una regla de lifetime que demuestre destrucción exacta. No se permite resolverlo mediante una
liberación implícita difícil de observar ni mediante ownership inferido que no aparezca en la
firma.

También deben recibir una decisión explícita:

- inicialización estática y su orden, o su exclusión de 1.0;
- propiedades sólo `set`, como feature soportada o rechazo sintáctico preciso;
- plegado de `const` locales derivados, sin dos evaluadores con reglas distintas;
- comportamiento de temporales owner que salen por `return`, llamadas y constructores;
- límites de `volatile`, dejando claro que no es sincronización;
- cualquier operación que hoy llegue al backend para descubrir que era inválida.

No todas tienen que implementarse. Todas tienen que terminar en una de tres respuestas comprobables:
funciona bajo un contrato, se rechaza con diagnóstico, o está declarada fuera de 1.0.

### 5.7 Gate de salida 0.7

- cero P0 y P1 conocidos en el lenguaje implementado;
- cero miscompile conocido;
- todas las fixtures positivas pasan en O0 y O2;
- todos los negativos fallan por el diagnóstico esperado;
- ownership no produce doble destrucción, fuga conocida aceptada ni use-after-move silencioso;
- el verificador de LLVM acepta todo módulo emitido;
- cada bug corregido tiene rojo anterior reproducido y verde posterior;
- ninguna prueba obtiene verde midiendo una función, fixture o compilador distinto.

---

## 6. Gate 0.8 — Toolchain comparable con trabajo nativo real

### 6.1 Proyectos y bibliotecas externas

El `.nsproj` debe expresar:

- fuentes y referencias entre proyectos;
- configuración y target;
- `.lib` externas;
- directorios de búsqueda;
- orden de bibliotecas;
- DLL importadas;
- recursos y outputs;
- opciones de linker explícitas y reproducibles.

Errores de símbolo ausente, arquitectura incompatible, biblioteca inexistente o ABI incorrecta
deben nombrar la entrada responsable.

### 6.2 Debug y Release como contratos

Definir qué cambia y qué no cambia:

| Propiedad | Debug | Release |
|---|---|---|
| Semántica observable | igual | igual |
| Optimización | baja | alta |
| Bounds checks | política explícita | política explícita |
| Assertions | activos | política documentada |
| Diagnósticos de ownership | completos | completos |
| Overflow | mismo contrato | mismo contrato |
| Debug info | completa | seleccionable |

Release no autoriza comportamiento indefinido. Quitar una comprobación exige demostrar la
precondición o aceptar una opción explícita del proyecto.

### 6.3 CodeView y PDB

Orden obligatorio:

1. archivo, línea y columna en IR;
2. nombres de funciones y scopes;
3. variables locales y parámetros;
4. tipos y layouts;
5. CodeView válido;
6. PDB utilizable en Visual Studio y WinDbg;
7. stack trace correcto a través de excepciones, callbacks C y métodos virtuales.

**Cierre:** colocar un breakpoint en código Native Sharp, inspeccionar una local, entrar a una
llamada, atravesar una excepción y obtener la línea correcta después de optimizar.

### 6.4 Build reproducible e identidad

- mismo input y toolchain producen el mismo binario;
- build id distingue snapshots sin cambiar la versión del lenguaje;
- manifiestos y metadata tienen versión única y centralizada;
- timestamps no contaminan outputs reproducibles;
- cada crash report identifica compilador, runtime, LLVM, linker y configuración;
- instalación limpia y desinstalación no dependen del árbol de desarrollo.

### 6.5 Paridad NSLink / `lld-link`

NSLink no necesita implementar todo COFF/PE. Necesita cubrir exactamente el dominio soportado y
fallar con claridad fuera de él.

- mismo conjunto de fixtures y programas;
- imports, exports, relocations, TLS, EH y debug sections soportadas;
- comparación estructural de PE;
- ejecución equivalente;
- mensajes útiles para símbolos y relocations no resueltos;
- ningún fallback silencioso que cambie el linker medido.

### 6.6 Gate de salida 0.8

Un repositorio nuevo, fuera de `A:\nsc-devel`, puede:

1. declarar un proyecto;
2. enlazar una biblioteca C externa;
3. producir Debug y Release;
4. ejecutarse en O0 y O2;
5. depurarse con símbolos;
6. reproducir el binario;
7. hacerlo siguiendo documentación pública, sin scripts privados.

---

## 7. Gate 0.85 — Plataforma de juego, no framework

### 7.1 COM estático

Native Sharp debe expresar las ABI necesarias para DirectX, WIC, WASAPI y DirectWrite:

- `Guid`, IID y CLSID de layout fijo;
- vtable por slots conocidos en compilación;
- `this` explícito en la frontera;
- `QueryInterface`, `AddRef` y `Release` expresables;
- `HRESULT` visible, sin convertirlo obligatoriamente en excepción;
- ownership documentado para interfaces COM;
- generación o consumo de bindings sin reflection.

No se crea un runtime COM. Se expresa la ABI que ya existe.

### 7.2 SIMD e intrínsecos

Agregar familias pequeñas guiadas por consumidores reales:

- vectores de 128 y 256 bits cuando el target lo soporte;
- load/store alineado y no alineado;
- operaciones de float e integer necesarias por matemáticas, audio y pixels;
- selección de target;
- fallback escalar cuando forme parte del contrato;
- IR inspeccionable para confirmar que el intrínseco sobrevivió hasta código máquina.

No se agrega una colección infinita de nombres de CPU. Cada familia necesita benchmark y uso real.

### 7.3 Frontera nativa completa

- structs y enums viajan en ambas direcciones con layout certificado contra clang-cl/MSVC;
- callbacks conservan calling convention y límite de excepciones;
- function pointers hacen round trip;
- strings cruzan como UTF-8/UTF-16 mediante APIs explícitas;
- ownership de buffers y handles queda escrito;
- errores de ABI se detectan antes del backend cuando sea posible.

### 7.4 Gate de salida 0.85

Una aplicación Native Sharp abre una ventana propia, recibe input, inicializa una API gráfica,
crea un dispositivo de audio, carga un asset y mantiene un loop sin thunks escritos a mano fuera
del sistema normal de bindings.

---

## 8. Gate 0.9 — El juego piloto

Las fixtures prueban contratos aislados. No prueban convivencia prolongada. Antes de congelar 1.0
debe existir un juego pequeño pero completo escrito en Native Sharp.

### 8.1 Vertical slice obligatorio

El programa debe incluir:

- ventana, mensajes e input;
- render real;
- audio real;
- carga de assets binarios;
- entidades o escena suficiente para ejercer ownership;
- structs matemáticos y SIMD;
- hilos de trabajo y sincronización;
- hot loop sin asignaciones accidentales;
- guardado de configuración en un formato propio sencillo;
- build Debug y Release;
- ejecución con `lld-link` y NSLink.

No necesita editor, engine, red, scripting dinámico ni servicios externos. Debe ser un consumidor,
no una nueva plataforma.

### 8.2 Presupuestos medidos

Antes de empezar se fijan presupuestos para:

- tiempo de frame promedio y percentiles altos;
- allocations por frame;
- memoria residente;
- tiempo de arranque;
- tiempo de compilación completa e incremental observado;
- tamaño de OBJ y EXE;
- tiempo de enlace;
- estabilidad después de horas de ejecución;
- costo de exceptions, virtual dispatch, bounds checks y ownership cleanup donde sean relevantes.

No se promete superar C++ por decreto. Se compara el mismo workload y se explica cada diferencia.

### 8.3 Soak, estrés y fallos

- ejecución prolongada;
- creación/destrucción repetida de recursos;
- pérdida y recreación de dispositivo gráfico;
- errores de asset;
- fallo de inicialización parcial;
- excepciones cruzando cleanup complejo;
- cierre durante trabajo concurrente;
- callbacks nativos durante shutdown;
- validación de ausencia de fugas y dobles liberaciones.

### 8.4 Congelación 0.9

Al entrar en beta se congelan:

- gramática;
- reglas de tipos;
- ownership y movimiento;
- mangling público;
- ABI del runtime;
- formato `.nsproj`;
- formato `.nsmeta`;
- CLI estable;
- comportamiento Debug/Release;
- lista de targets soportados.

Desde aquí, cualquier cambio incompatible exige migración documentada y aumento de versión.

---

## 9. Gate 1.0 — Contrato público

### 9.1 Lenguaje

- especificación 1.0 suficientemente precisa para decidir si un programa es válido;
- contrato de enteros, floats, overflow, casts y división;
- modelo de memoria y atomics;
- lifetime, ownership, borrowing y move;
- excepciones y orden de cleanup;
- layout y calling conventions;
- inicialización global/estática determinista o exclusión explícita;
- lista cerrada de comportamiento dependiente del target;
- cero regla semántica importante definida únicamente por una prueba o comentario del código.

### 9.2 Compilador

- cero P0/P1 conocidos;
- cero miscompile conocido;
- errores de usuario no provocan crash ni IR inválido;
- O0 y O2 producen la misma semántica observable;
- recuperación del parser no genera cascadas absurdas;
- diagnósticos principales incluyen fichero, línea, columna y causa;
- el backend verifica el IR antes de emitir artefactos de release;
- versiones LLVM soportadas fijadas y probadas;
- suite negativa tan importante como la positiva.

### 9.3 Toolchain

- instalador o paquete reproducible;
- `nsc`, runtime, Core, NSLink y herramientas con versiones compatibles;
- compilación desde una máquina limpia documentada;
- PDB y depuración funcional;
- `.lib` externas y proyectos múltiples;
- builds Debug/Release;
- logs suficientes para reproducir un fallo;
- actualización y desinstalación sin dejar dependencias ocultas.

### 9.4 Compatibilidad

- Semantic Versioning para compilador y lenguaje;
- versiones independientes para `.nsmeta` y ABI del runtime;
- política de deprecación;
- compatibilidad hacia atrás dentro de 1.x salvo bug de seguridad o miscompile;
- un error anterior no se conserva sólo por compatibilidad si produce corrupción silenciosa;
- build metadata para identificar snapshots sin fingir una versión nueva del lenguaje.

### 9.5 Evidencia final

1. Suite completa verde en una instalación limpia.
2. Todas las fixtures ejecutadas en O0 y O2.
3. Matriz `lld-link`/NSLink verde.
4. ABI comparada con clang-cl/MSVC.
5. Juego piloto compilado, depurado y ejecutado durante soak.
6. Cero fuga o doble destrucción conocida en el corpus.
7. Binarios reproducibles.
8. Manual del lenguaje, guía de ownership, guía de FFI y tutorial del juego.
9. Release candidate usada sin cambios semánticos durante un periodo de estabilización.
10. Lista pública y corta de limitaciones aceptadas que no contradicen la promesa.

Sólo después de esas diez pruebas se cambia la versión a `1.0.0`.

---

## 10. Cosas que no bloquean 1.0

- self-hosting;
- abandonar LLVM o construir un segundo backend;
- soportar Linux, macOS, móviles o consolas;
- implementar todo C#;
- templates al nivel de C++ o metaprogramación al nivel de D;
- LTO/ThinLTO sin evidencia de que haga falta;
- linker incremental sin tiempos que lo justifiquen;
- reflection;
- GC;
- ejecución dinámica de código;
- package manager universal;
- engine incluido;
- editor incluido;
- web, SQL, bases de datos o aplicaciones empresariales;
- paridad de biblioteca con .NET, Java o C++.

Windows x64 puede ser el único target de 1.0. Una plataforma bien soportada vale más que cinco
targets nominales.

---

## 11. Disciplina de cada entrega

Toda corrección o feature aceptada sigue este orden:

1. Consumidor real o defecto reproducible.
2. Programa rojo mínimo.
3. Regla semántica escrita.
4. Una fuente de verdad en el compilador.
5. Unit test de la capa afectada.
6. Fixture de punta a punta.
7. Caso negativo con diagnóstico exacto.
8. Inspección de HIR/MIR/LLVM cuando la forma importe.
9. Ejecución Windows O0/O2 cuando el resultado importe.
10. Corrida completa para detectar regresiones.

Además:

- una prueba no cuenta si sólo afirma su cobertura en un comentario;
- una aserción de ausencia necesita guardia de que encontró la función o sección correcta;
- una fixture no cuenta si usó un compilador distinto al registrado;
- compilar no sustituye ejecutar;
- ejecutar correctamente no sustituye comprobar destrucción, llamadas o forma del IR cuando esos
  son parte del contrato;
- compartir implementación no sustituye probar caminos semánticamente distintos;
- no se agregan features por comparación de checklist con otro lenguaje.

---

## 12. Orden recomendado

```text
0.7.1  Certificar c41-c43 en Windows y cerrar la entrega actual
0.7.2  NSC-LIMIT-005: observabilidad y corrección O2
0.7.3  NSC-LIMIT-007: constraints genéricos mínimos
0.7.4  NSC-LIMIT-009: tipos anidados
0.7.5  Consolidación semántica y cero P0/P1

0.8.0  .nsproj externo + .lib + Debug/Release
0.8.1  CodeView/PDB
0.8.2  builds reproducibles + identidad + instalación limpia
0.8.3  paridad NSLink/lld-link en el dominio soportado

0.8.5  COM estático
0.8.6  SIMD e intrínsecos guiados por consumidor
0.8.7  aplicación completa de ventana/input/render/audio/assets

0.9.0  juego piloto y congelación de lenguaje/ABI
0.9.x  bugs, rendimiento, soak, documentación y compatibilidad
1.0.0-rc.1  instalación externa y periodo de estabilización
1.0.0  contrato público
```

El orden puede cambiar por un bug bloqueante o por evidencia del juego piloto. No cambia por el
deseo de añadir una feature llamativa.

---

## 13. Definición de «1.0 Glory»

La gloria no es que Native Sharp tenga más palabras clave que C++, más biblioteca que C# o más
plataformas que Java.

La gloria es ésta:

> Un programador abre un proyecto vacío, escribe un juego nativo con sintaxis clara, ve y controla
> sus costos, llama directamente a la plataforma, depura código fuente real, obtiene el mismo
> comportamiento en Debug y Release, y puede confiar en que ownership, ABI y optimización no van a
> traicionarlo.

Si Native Sharp consigue eso sin GC, sin reflection dinámica, sin runtime pesado y sin convertirse
en una plataforma web/SQL/enterprise, entonces 1.0 habrá cumplido exactamente la promesa correcta.
