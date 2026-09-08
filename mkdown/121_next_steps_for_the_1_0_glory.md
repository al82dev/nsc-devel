# 121 — Next Steps for the 1.0 Glory

**Fecha:** 2026-09-08
**Punto de partida:** `nsc 0.9.0` instalado y certificado con las megaentregas E1–E6 rumbo a 1.0.
NativeSharp 0.1 / Stage 57. La identidad `+build.<UTC>.<hash>` se genera de nuevo para cada binario
y se registra al certificar.
**Objetivo:** llevar Native Sharp a 1.0 como lenguaje y toolchain para software nativo con
rendimiento predecible, sin ligar su promoción a consumidores externos ni a catálogos de bindings
que todavía evolucionan por separado.

**Estado de la entrega más reciente:** `nsc 0.9.0+build.20260908T152440Z.079cf744116ec119`
está instalado y certificado como NativeSharp 0.1 / Stage 67. G00
`20260908-092439-7d0bd312` terminó `LANDED` con 12/12 etapas, 304/304 CTest por enlazador,
324/324 casos y cero fallos. `S0 == S1` sobre 1035 archivos y `built_nsc == installed_nsc` con
SHA-256 `f7e920496191650c460ff580ed49d05e0cbefb6356d685f2a7784c28b65668e6`. La implementación
didáctica está en 153 y el acta exacta en 154.

**Baseline anterior Stage 64:**
`nsc 0.9.0+build.20260908T023103Z.84b08f1eced9c7f3` está instalado y certificado como NativeSharp
0.1 / Stage 64. G00 `20260907-203102-950828a1` terminó `LANDED`, `certified: true`, con **0
fallos**, **12/12 etapas** y **324/324 casos**. Las matrices CTest dieron **302/302** con lld-link
y **302/302** con NSLink. `built_nsc` e `installed_nsc` comparten el SHA-256
`e9cec52543a6cd709f41b085a9a119c46266b40dfa34dab56cedc9a4a18124fd`; el árbol permaneció
inmóvil en sus 1029 archivos medidos. Esta entrega promueve el inventario criptográfico completo de
la instalación; el acta exacta está en 149.

**Cierre de 0.9.0:** la frontera `.lib`/DLL cubre composición transitiva, llamadas
NativeSharp sin redeclaración, structs y enums con ABI exacto, receptores de instancia de structs,
clases opacas con ownership y métodos concretos, y callbacks `extern C delegate`. NSMETA 1.5
conserva ahora la firma de callbacks y sigue leyendo 1.0–1.4. Stage 46 suma **111 checks** por
linker. El catálogo final da **297/297 efectivos con lld-link y 297/297 con NSLink**. El producto es
`nsc 0.9.0+build.20260907T042535Z.03ca780e6debcd15`, SHA-256
`ADB3BAF106CDC666D7E005436280FE2DECC7362530FB2E13D0057EF794622FC1`. Ésa es la identidad histórica
del cierre técnico descrito en `129_cierre_native_sharp_0_9_0.md`; las megaentregas posteriores
reconstruyeron el compilador y el baseline instalado vigente se registra abajo.

**Avance certificado hacia 1.0 (2026-09-07):** E1–E4 ya están implementadas. E1 publica objetos,
constructores, métodos de instancia, jerarquía y destruction entries; E2 transporta templates
públicos y especializaciones genéricas; E3 lleva delegates ordinarios, closures y excepciones
tipadas a través de `.lib` y DLL con cleanup del productor; E4 congela Native ABI 1.0 y Runtime
ABI 1.0 en NSMETA 1.8, con golden fingerprint y rechazo temprano de contratos incompatibles. La
historia técnica y la matriz exacta están en los documentos 132–136. El baseline G00
`20260907-112232-d023cfe0` terminó `LANDED`, `certified: true`, con **0 fallos**, **11 etapas** y
**324 casos**. El árbol permaneció inmóvil (`S0 == S1`), y `built_nsc == installed_nsc` con SHA-256
`dc56280ddf658bc798e9258d80facfeb72748516265cc3a316d4060317955e00`. Esto reemplaza el estado
local provisional: E1–E4 están integradas en el compilador instalado y certificadas con lld-link y
NSLink.

**Avance certificado E5–E6 (2026-09-07):** E5 sustituye el rechazo ABI genérico por diagnósticos que
nombran contrato, valor esperado y valor observado; además valida identidades duplicadas y tablas
estructuralmente incoherentes antes de materializar un consumidor. E6 añade
`--verify-library-artifacts`, que certifica en una sola operación la relación entre `.nsmeta`, el
archivo COFF y, cuando existe, la tabla de exports de la DLL. El cierre técnico local terminó
**297/297**; después, el G00 `20260907-131523-7eda857d` reconstruyó, instaló y certificó el árbol
completo con **324 casos**, ambos enlazadores y 0 fallos. La implementación está en 137 y 138; el
acta instalada, con identidades, hashes y lectura de la evidencia, está en 139.

**Avance certificado E7 (2026-09-07):** la semántica que estaba repartida entre el libro, documentos
históricos, pruebas y comentarios ya tiene una autoridad normativa única. La especificación 1.0 y
su gramática EBNF cubren 42 cláusulas independientes; un mapa conecta cada una con archivos de
implementación y evidencia ejecutable. Stage 60 valida esa trazabilidad con **954 checks** en cada
árbol. Las 37 suites focales citadas pasaron 37/37, primero con lld-link y después con NSLink. La
especificación se instala con el producto bajo `share/nativesharp/spec`. El detalle técnico está en
140. El G00 `20260907-152255-f7e1f291` promovió el cierre: **11 etapas, 324 casos y 0 fallos**, con
árbol inmóvil y ejecutable construido idéntico al instalado. El acta exacta está en 141.

**E8 certificada (2026-09-07):** Stage 61 inicia el endurecimiento del compilador con tres límites
operativos: un máximo predeterminado de 100 errores retenidos por compilación, una frontera que
convierte excepciones C++ no controladas en diagnóstico y exit 70, y una única línea LLVM soportada
—23.x— comprobada al configurar y contra la biblioteca realmente cargada. El comando
`--print-llvm-version` hace visible esa identidad. Ocho cláusulas del contrato del compilador quedan
trazadas a código y evidencia; la suite nueva pasa 95 checks y la selección focal, 10/10 en cada
árbol. La matriz local cubrió 299/299 casos en ambos caminos de enlace. G00
`20260907-170412-16b5b8cb` promovió después el Stage 61: 11 etapas operativas, 324 casos, cero
fallos, árbol inmóvil y binario construido idéntico al instalado. El desarrollo está en 142 y el
acta de certificación, en 143.

**E9 certificada hasta Stage 64 (2026-09-07):** Stage 62 crea el contrato reproducible del toolchain. El propio
`nsc` genera, imprime, escribe, verifica e instala `NativeSharpToolchain.contract`; el artefacto
concentra las versiones de compilador, lenguaje, LLVM, NSMETA y ABI, además de la superficie
certificada. G00 `20260907-175537-29408b6e` la promovió con 324/324 casos, cero fallos, árbol
inmóvil y binario construido idéntico al instalado. El método y los 43 checks están en 144; el acta
de promoción está en 145. Stage 63 añade verificación del layout instalado ejecutada por el propio
`nsc.exe` recién copiado y corrige la guía de construcción limpia. G00
`20260907-190718-3fc07605` promovió Stage 63 con 301/301 CTest por linker, 324/324 casos del runner,
cero fallos, árbol inmóvil y ejecutable construido idéntico al instalado. La implementación está
en 146 y el acta de promoción en 147. Stage 64 deriva del origen una lista de 76 archivos públicos,
genera tamaño y SHA-256 de cada uno y hace que el `nsc.exe` instalado rechace ausencia, alteración o
residuos dentro de los árboles que el paquete posee. G00 `20260907-203102-950828a1` promovió esa
fotografía con 302/302 CTest por linker, 324/324 casos del runner, cero fallos, árbol inmóvil y
ejecutable construido idéntico al instalado. El método completo está en 148 y el acta en 149.

**E9+E10 certificadas (2026-09-07):** Stage 66 absorbe la etapa intermedia y entrega en
una sola fotografía la comparación de builds independientes, enlace PE reproducible, planes
`NSUPDATE 1` ligados a los manifiestos anterior y siguiente, retiro con preflight y preservación de
archivos modificados, y la política instalada `NSCOMPAT 1`. Dos construcciones desde cero
coincidieron en las 76 entradas deterministas y, usando una identidad reservada únicamente para el
gate, produjeron `nsc.exe` completos con el mismo SHA-256. Una reinstalación física retiró un archivo
obsoleto demostrado como propiedad anterior y verificó el árbol final. El desarrollo, los
hallazgos y la evidencia de implementación están en 150. G00 `20260907-233625-801b209e` promovió
la fotografía con 303/303 CTest por linker, 324/324 casos, cero fallos, árbol inmóvil y compilador
construido idéntico al instalado. El acta de certificación está en 151.

**E11 certificada (2026-09-08):** Stage 67 reemplaza la captura por copia por celdas léxicas
compartidas con lifetime contado, mutación visible entre lambdas hermanas, anidamiento y capturas
transitivas. Añade parámetros explícitos y por referencia, participación en overload resolution,
captura de structs grandes y fixed arrays, cleanup normal/excepcional y lambda sin captura como
callback C. G00 `20260908-092439-7d0bd312` promovió la entrega con 304/304 CTest por enlazador,
324/324 ejecuciones, cero fallos, árbol inmóvil y compilador construido idéntico al instalado. La
implementación está en 153 y la certificación en 154.

**Incidente de validación de 0.8.0-alpha:** CTest llegó primero a **297/297 verde**, pero el runner externo reveló
que ese arnés no ejecutaba `c25`–`c28` en Windows. Las cuatro fixtures sufrían un doble free en el
crecimiento de campos `owned T[]`, tanto en O0/O2 como con lld-link/NSLink. La corrección mantiene
`raii T[]` y restaura para campos array el contrato explícito `delete[] campo; campo = reemplazo;`.
El baseline certificado posterior ejecuta las cuatro fixtures y obtiene 42 en ambos niveles y con
ambos linkers. La regresión de IR que impide restaurar el doble free pasa 32/32.

---

## 1. La promesa de Native Sharp 1.0

Native Sharp 1.0 debe permitir escribir, compilar, enlazar, depurar y distribuir software nativo
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
- suficiente expresividad para construir módulos nativos, bibliotecas y herramientas de alto
  rendimiento sin heredar la complejidad histórica completa de C++.

1.0 no significa «tener muchas features». Significa que lo que el lenguaje promete funciona de
punta a punta, es diagnosticable y puede sostener un proyecto real.

### 1.1 Compatibilidad dirigida con C#

Native Sharp debe ser lo más compatible posible con C# **sin convertirse en C#**. La meta es que
un programador de C# reconozca la gramática, el sistema nominal, las expresiones y el comportamiento
ordinario sin reaprender diferencias arbitrarias. No se busca compatibilidad binaria con .NET ni
copiar su runtime, su biblioteca completa o todos sus dominios de aplicación.

Regla de diseño:

1. Si la semántica de C# puede implementarse sin GC, reflection dinámica, runtime pesado ni costos
   ocultos incompatibles con el objetivo nativo, Native Sharp conserva esa sintaxis y semántica.
2. Si ownership, lifetime, layout, ABI o rendimiento predecible exigen una diferencia, Native Sharp
   la hace visible en el código y la documenta.
3. Si una forma familiar no está soportada, el frontend la rechaza con un diagnóstico preciso; no
   debe aceptarla para producir un resultado silenciosamente distinto.
4. Una diferencia sólo se acepta por una propiedad central del lenguaje nativo, no por comodidad
   accidental de la implementación actual.

Esto favorece compatibilidad en operadores, conversiones ordinarias, control de flujo, arrays,
genéricos, tipos anidados y APIs pequeñas de Core. Las divergencias intencionales se concentran en
`owned`, `borrowed`, `raii`, `move`, punteros, destrucción determinista, layout y fronteras ABI.

Por tanto, «compatible con C#» significa **familiaridad de fuente y comportamiento donde el costo
es honesto**. No significa GC, LINQ, reflection, ASP.NET, SQL, ecosistema .NET ni paridad total de
biblioteca.

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

Una necesidad concreta de un consumidor puede resolverse mediante una biblioteca externa o un binding
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
- delegates y eventos; las clausuras completas quedaron implementadas y certificadas por E11 /
  Stage 67;
- arrays, fixed arrays, spans, colecciones y `foreach`;
- unions, pack, align, offsets y ABI Win64;
- enums con tipo subyacente y nombres sin reflection;
- FFI C, function pointers, callbacks e importación desde DLL;
- hilos, atomics y primitivas de sincronización;
- UTF-8 explícito;
- emisión de OBJ, EXE, LIB y DLL;
- frontend, HIR, MIR y backend LLVM;
- enlace por `lld-link` y NSLink.

Las correcciones del documento 120 y los nuevos hallazgos reportados por consumidores externos
permitieron trabajar estas interacciones de madurez:

- receptor struct producido por propiedad o indexador;
- transferencia de ownership hacia constructores;
- igualdad de structs por hojas declaradas, sin padding.
- reenvío correcto de parámetros `out`;
- constraints genéricos dentro de una compilación;
- tipos anidados, incluido lookup y uso desde consumidores reales.

Los proyectos externos pueden descubrir combinaciones que las fixtures todavía no cubren y enviar
repros mínimos. Ahí termina su papel dentro de este roadmap: sus fuentes, workarounds, builds y
suites pertenecen a esos proyectos y no son responsabilidad ni gate de Native Sharp. Cada defecto
aceptado debe convertirse en una prueba autocontenida dentro de `nsc`.

### 3.1 Lectura actual después de los nuevos reportes

| Hallazgo | Estado | Lectura para 1.0 |
|---|---|---|
| NSC-LIMIT-004 | Corregido | igualdad de agregados dejó de producir LLVM inválido |
| NSC-LIMIT-005 | Corregido y certificado en 0.8.0-alpha | observación del pipeline, fixture diferencial y matriz O0/O2 pasan con ambos linkers |
| NSC-LIMIT-006 | Corregido | `out` ya conserva definite assignment al reenviarse |
| NSC-LIMIT-007 | Corregido localmente | existen constraints; falta completar la frontera de proyecto |
| NSC-LIMIT-008 | Corregido | los temporales struct producidos por propiedades son utilizables |
| NSC-LIMIT-009 | Corregido | los tipos anidados ya son parte efectiva del lenguaje |
| NSC-LIMIT-010 | Corregido | constructores participan correctamente en transferencia `owned` |
| NSC-LIMIT-011 | Corregido y certificado | constraints cross-project pasan pruebas propias y el repro consumidor exacto |
| NSC-LIMIT-012 | Corregido en 0.7.6 | `|`, `&`, `^`, `~` y asignaciones compuestas conservan el enum; mezcla de enums se rechaza |
| NSC-LIMIT-013 | Corregido en 0.7.6 | el parser exige la forma real `tipo identificador`; `GL Gl; Gl = gl;` y referencias estáticas coexisten |
| NSC-LUNAR-007 | Corregido en 0.7.6 | el backend normaliza el contador al ancho LLVM del valor en shifts simples y compuestos |
| NSC-LIMIT-014 | Excluido explícitamente de 0.8 | `static owned` sigue rechazado: sin GC ni finalización de módulo no existe una destrucción honesta; se conserva ownership manual explícito |
| NSC-LIMIT-015 | Corregido en 0.7.6 | el RHS se evalúa antes de destruir el owner desplazado; clases y strings pasan O0/O2 |
| NSC-LIMIT-016 | Corregido en 0.8.0-alpha | `raii T[]` posee y libera el buffer, admite retorno owned y `move`; los elementos conservan la semántica explícita de arrays nativos |
| NSC-LIMIT-017 | Corregido en 0.8.0-alpha | `new T[] { ... }` infiere longitud y evalúa/almacena una vez, de izquierda a derecha |
| NSC-LIMIT-018 | Corregido en 0.8.0-alpha | las firmas genéricas son visibles, pero los cuerpos ordinarios sólo se instancian al ser llamados o convertidos a delegate |
| NSC-LIMIT-019 | Corregido en 0.7.6 | bindings en dos pasos, plegado calificado/transitivo/cross-project y detección real de ciclos |
| NSC-LIMIT-020 | Corregido en 0.8.0-alpha para código fuente | métodos, constructores y encadenamiento admiten defaults literales; metadata precompilada queda como cierre posterior |
| NSC-LIMIT-021 | Corregido en 0.8.0-alpha | un owner string fresco puede prestarse durante la expresión completa con cleanup normal y excepcional exacto |
| NSC-LIMIT-022 | Corregido después del baseline 0.8.0-alpha; recertificación pendiente | la identidad de un argumento genérico se deriva de su declaración canónica y se conserva al clonar; productor y consumidor comparten una monomorfización |
| CORE-LUNAR-003 / BIND-LUNAR-002 | Abierto, P2 de plataforma | `DateTime` y el binding Win32 no cubren calendario/hora local |

La evolución es correcta: Native Sharp dejó de acumular sintaxis aislada y empezó a corregir las
interacciones que aparecen en software nativo real. **0.8.0-alpha** cierra los workarounds de
compilador que bloqueaban un consumidor externo salvo una exclusión deliberada: el estado global
`static owned` no forma parte del contrato mientras no exista finalización de módulo real.
La identidad `0.8.0-alpha` ya obtuvo certificación instalada. Promoverla a `0.8.0` cambia la
identidad del artefacto y requiere repetir el baseline una última vez; no requiere añadir features.

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

### 0.9.x-beta — congelación y consumidores externos

La sintaxis, la semántica, el formato de proyecto y las ABI públicas se congelan. El corpus del
compilador y consumidores externos se usan para descubrir interacciones que las fixtures aisladas
no ven, sin convertir a esos consumidores en parte del producto.

### 1.0.0 — promesa de compatibilidad

La versión deja de describir progreso interno y pasa a ser un contrato con usuarios: el código
válido no cambia de significado silenciosamente y los formatos versionados no cambian sin una
regla de compatibilidad.

---

## 5. Gate 0.7 — Correctness antes que superficie

### 5.1 Certificar la entrega actual

La primera matriz G00 descubrió correctamente que su propio orden de medición era inválido. Tras
mover la fotografía después de `_build_tests()`, la repetición terminó `LANDED` y certificada, sin
fallos ni mutaciones del árbol medido. Esa certificación cierra el baseline anterior. Cada nuevo
binario —incluido 0.7.6— debe repetir el gate con su propia identidad; no basta reutilizar el
resultado anterior.

- reservar una ventana sin builds concurrentes o usar directorios de build aislados por corrida;
- reconstruir e instalar el `nsc.exe` exacto que se va a medir;
- terminar de producir todos los ejecutables de prueba antes de capturar el inventario inicial;
- registrar nombre, tamaño y hash de cada herramienta/fixture al inicio y al final;
- ejecutar CTest completo;
- ejecutar las fixtures CC1 en O0 y O2 sobre Windows real;
- confirmar los resultados de c41, c42 y c43;
- correr con `lld-link` y NSLink cuando la ruta probada involucre enlace;
- registrar build id, commit, LLVM, linker y resultado;
- no declarar verde una prueba que usó otro compilador instalado;
- invalidar toda matriz donde aparezca, desaparezca o cambie un ejecutable, aunque el contador de
  fallos funcionales sea cero.

### 5.2 Cerrar `NSC-LIMIT-005`: observabilidad del optimizador

**Estado 0.8.0-alpha:** cerrado y certificado por G00 con lld-link/NSLink y O0/O2.

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

### 5.3 `NSC-LIMIT-011` — cerrado y conservado como contrato

**Estado al 2026-09-05:** implementación, pruebas propias, matriz G00 y repro consumidor exacto
verdes. El detalle técnico está en el documento 125. La retirada de workarounds en proyectos
externos corresponde exclusivamente a sus mantenedores.

`NSC-LIMIT-007` ya demostró que el lenguaje parsea y valida las constraints mínimas necesarias para
APIs nativas, engines y matemáticas reutilizables:

```csharp
where T : class
where T : BaseClass
where T : IInterface
where T : class, IInterface
```

No se construye un solver general. Las constraints son comprobaciones estáticas sobre
especializaciones monomorfizadas. El hueco restante es más estrecho y más importante: una
plantilla constrained declarada en una biblioteca no puede cerrarse con un tipo que sólo existe en
el proyecto consumidor.

El caso contractual es:

```csharp
// Library
public static T Identity<T>(T value) where T : class, IComponent;

// Consumer
public class Payload : IComponent { }
Payload output = Catalog.Identity<Payload>(input);
```

El defecto observado intentaba resolver `Payload` dentro del módulo de la biblioteca. Esto rompe
una capacidad propia del lenguaje: una biblioteca extensible debe aceptar implementaciones válidas
de sus interfaces declaradas por cualquier proyecto consumidor.

Se requiere:

- preservar `originProjectOrdinal` y `originNamespace` del sitio consumidor en
  `GenericConstraintClause::substitutedArgument` durante clonación y re-clonación;
- aplicar la regla «propagar, nunca re-derivar» en especializaciones anidadas;
- resolver el argumento concreto con la visibilidad del consumidor y el constraint con la
  visibilidad de la biblioteca;
- mantener la identidad de especialización separada de la calificación usada por name lookup;
- diagnosticar tipos inválidos o inaccesibles en el sitio consumidor y nombrar el módulo correcto;
- probar métodos y tipos genéricos con `class`, interfaz, clase base y combinaciones;
- cubrir argumentos válidos, inválidos e inaccesibles, especialización anidada, tipos homónimos en
  dos consumidores y descarte de overloads por constraint;
- validar HIR, MIR, LLVM y emisión PE O0; después incluir el caso en la certificación O2;
- ninguna metadata dinámica ni diccionario de tipos en runtime.

**Cierre:** los repros propios de `nsc` cubren el método y el tipo genérico cross-project, pasan de
punta a punta y permanecen en CTest. Ningún proyecto externo es requisito para declarar el cierre.

### 5.4 Certificar `NSC-LIMIT-009`: tipos anidados

Los tipos anidados ya funcionan. Tienen valor para organizar bindings grandes y representar APIs
de plataforma; no entraron por paridad con C#.

Se requiere:

- identidad `Outer+Inner` estable;
- lookup lexical y calificado;
- accesibilidad completa;
- mangling estable;
- soporte en genéricos, herencia, metadata, C headers y diagnósticos;
- pruebas con dos tipos internos del mismo nombre en contenedores diferentes;
- ninguna búsqueda dependiente del orden de archivos.

El trabajo pendiente para 0.7 no es volver a implementarlos, sino mantener sus fixtures como
contrato y certificar que no divergen en O0/O2 ni entre proyectos.

### 5.5 Cerrar `NSC-LIMIT-012`: bitwise de enums tipados

**Estado 0.7.6:** cerrado. La suite propia ejecuta las cinco formas sobre local y propiedad en O0
y O2, y una fixture negativa exige el diagnóstico exacto al mezclar enums distintos.

Para flags de APIs nativas, dos operandos del mismo enum deben conservar el tipo enum:

```csharp
ClearBufferMask mask = ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit;
```

Se requieren `|`, `&`, `^`, `~` y sus asignaciones compuestas, siempre que los operandos sean del
mismo enum compatible. Mezclar enums distintos debe fallar con un diagnóstico preciso. No se
agrega reflection de enums, conversión dinámica ni un subsistema de metadata: es aritmética tipada
necesaria para Win32, OpenGL, DirectX y otras ABI nativas.

### 5.6 Cerrar `NSC-LIMIT-013`: colisión entre valor y tipo importado

**Estado 0.7.6:** cerrado. La causa era la existencia simultánea de `GL` y un tipo importado real
`Raw.Gl`, no una comparación case-insensitive. El parser ahora reconoce una declaración sólo si
después del tipo existe un identificador de variable; el repro de dos proyectos y una referencia
estática legítima permanecen en la suite.

El lookup de una expresión debe preferir la coincidencia exacta de una variable, parámetro, campo o
propiedad accesible antes de reinterpretarla como un tipo importado cuya capitalización difiere.
El patrón `GL Gl; Gl = gl;` debe ser válido también cuando `GL` viene de un `ProjectReference`, sin
romper referencias estáticas legítimas a `GL`.

### 5.7 Promover y cerrar `NSC-LUNAR-007`: ancho del operando derecho en shifts

**Estado 0.7.6:** cerrado para el defecto reportado. El tipo semántico del contador no cambia; el
backend lo normaliza al ancho físico del operando izquierdo antes de emitir `shl`, `lshr` o
`ashr`. La fixture cubre `ulong`/`int`, izquierda, derecha, `<<=` y `>>=` en O0/O2. La política de
conteos fuera de rango sigue siendo la trampa explícita de NativeSharp ya fijada por CC1; no se
delega a poison de LLVM.

El frontend acepta una expresión natural para bitsets nativos:

```csharp
ulong mask = 1UL << index; // index es int
```

El backend genera actualmente la forma inválida `shl i64 ..., i32 ...`. En LLVM, ambos operandos
de la instrucción de desplazamiento deben tener el mismo tipo entero. Tipar también el contador
como `ulong` puede desbloquear al consumidor, pero es sólo un workaround y no forma parte del
contrato deseado del lenguaje.

Se requiere:

- normalizar el operando derecho al tipo LLVM exigido por el operando izquierdo antes de emitir
  `shl`, `lshr` o `ashr`;
- aplicar la misma regla a `<<`, `>>`, `<<=` y `>>=`;
- conservar la semántica de C# para el conteo: usar los cinco bits bajos en operandos de 32 bits y
  los seis bits bajos en operandos de 64 bits, sin delegar valores negativos o grandes a poison de
  LLVM;
- cubrir combinaciones de enteros con distinto ancho, firmados y no firmados;
- añadir el patrón real de bitset `ulong << int` como fixture positiva propia de `nsc`;
- comprobar HIR, MIR, LLVM verificado, ejecución O0/O2 y que nunca se emitan operandos de tipos
  distintos.

**Cierre:** toda expresión de shift aceptada por el frontend baja a LLVM válido y produce el mismo
resultado observable en O0 y O2. Un caso no soportado debe rechazarse antes del backend con un
diagnóstico del compilador. Hasta que exista una entrada canónica en la carta de `nsc`, se
conserva el identificador del reporte externo y no se reutiliza un número `NSC-LIMIT`.

### 5.8 Resolver `NSC-LIMIT-014`: ownership del estado estático

El compilador prohíbe correctamente una declaración como ésta bajo su modelo actual:

```csharp
private static owned Resource shared;
```

La razón no es parsing ni HIR incompleto: Native Sharp todavía no genera destrucción para estado
estático. Aceptar el campo como `owned` sin ejecutar su cleanup violaría la promesa central del
calificador y convertiría una fuga de recursos en comportamiento aparentemente seguro. Que el
programa tenga conteo de referencias y una ruta manual de liberación no permite al compilador
suponer que esa ruta se ejecutará exactamente una vez.

El workaround controlado es mantener una referencia estática no propietaria y administrar de
forma explícita la transferencia, el conteo y el `delete` al llegar a cero. Debe documentarse como
salida temporal del verificador de ownership, con estos riesgos visibles:

- fuga si no se alcanza la liberación final;
- doble destrucción si dos rutas creen poseer el mismo recurso;
- referencia estática colgante si no se invalida al destruir;
- carrera entre adquisición y liberación si el estado se usa desde varios hilos;
- cleanup incompleto ante inicialización parcial o excepción.

Para soportar `static owned`, se requiere:

- definir cuándo se inicializa el estado estático y en qué orden entre tipos, archivos y proyectos;
- generar destrucción exactamente una vez al terminar normalmente el proceso;
- definir el orden inverso de cleanup y el acceso entre estáticos durante shutdown;
- cubrir reemplazo del valor, movimiento, liberación explícita, inicialización fallida y excepción;
- impedir que `delete` manual y cleanup generado puedan destruir dos veces el mismo owner;
- documentar qué garantías no existen ante terminación abrupta del proceso;
- probar contadores de construcción/destrucción, múltiples proyectos y ejecución O0/O2.

Hay dos cierres válidos para 1.0: implementar esa semántica completa o declarar `static owned`
fuera de 1.0 manteniendo el rechazo preciso. El workaround sólo puede retirarse en el primer caso,
cuando una fixture propia demuestre destrucción determinista y única.

### 5.9 Corregir `NSC-LIMIT-015`: overwrite de campo `owned`

**Estado 0.7.6:** corregido el P0 reproducible. El lowerer obtiene la dirección del destino,
evalúa el RHS completo y sólo después destruye el owner desplazado. Dos regresiones de ejecución
—clase con contadores y `string` con `String.Concat(text, value)`— pasan en O0/O2, además del repro
consumidor exacto a través de LLVM verificado.

El repro mínimo confirmó un P0: `text = String.Concat(text, value)` compila en HIR/LLVM y enlaza
en O0, pero libera `text` antes de evaluar el RHS que todavía necesita leerlo. El programa imprime
`"c"` y devuelve 8 en lugar de conservar `"abc"` y devolver 42. Es un use-after-free y un
resultado incorrecto en código aceptado, no una regla pendiente de diagnóstico.

El repro mínimo debe separar:

- asignación directa contra `raii` + `move`;
- target local contra campo `owned`;
- RHS que toma prestado el valor anterior contra RHS independiente;
- `string` contra una clase poseída pequeña con contadores de construcción y destrucción;
- inicialización del campo contra reemplazo de un owner ya existente;
- resultado funcional, destrucción exactamente una vez y ausencia de uso después de liberar;
- ejecución O0 y O2, además de inspección MIR/LLVM de la transferencia.

La asignación debe bajar en este orden: obtener una dirección estable del destino; evaluar y
materializar completamente el RHS; leer y anular el owner anterior; destruirlo; guardar el owner
nuevo sin que el cleanup del temporal vuelva a liberarlo. Esto conserva el orden de evaluación de
C# sin debilitar el ownership explícito de Native Sharp.

**Cierre:** el repro directo devuelve 42, una clase poseída con contador demuestra destrucción
única y las variantes `this.field`, `other.field`, propiedad, excepción y alias indirecto pasan
en O0/O2. Este P0 debe cerrarse antes que cualquier feature nueva.

### 5.10 Cerrar `NSC-LIMIT-016`: arrays poseídos como locales `raii`

**Estado 0.8.0-alpha:** cerrado con semántica de buffer nativo. `raii T[]` libera exactamente el
buffer, participa en unwind y puede transferirse con `move`. No recorre ni destruye implícitamente
los elementos: un array NativeSharp sigue siendo almacenamiento de bajo nivel y la propiedad de
sus elementos continúa explícita. Esa decisión evita introducir comportamiento de GC disfrazado.

El frontend rechaza una local como:

```csharp
raii Item[] loaded = LoadItems();
```

El modelo actual conserva `owned T[]` como buffer relocalizable, pero no promete recorrer y
destruir sus elementos al salir del scope. El workaround puede usar asignación directa cuando el
RHS no lee el destino anterior, o `owned T[]` más `delete[]` explícito; ninguna de las dos formas
ofrece por sí sola exception safety equivalente a una local RAII.

Se requiere cleanup del bloque y de sus elementos en orden definido, tracking de slots movidos o
nulos, `move(array)`, unwind de inicialización parcial y prevención de doble ownership al copiar
referencias. Las fixtures deben cubrir arrays vacíos/parciales, elementos triviales y poseídos,
transferencia completa, excepción y destrucción exactamente una vez en O0/O2.

**Cierre:** una local `raii T[]` puede recibir un retorno owned, sobrevivir a fallos intermedios y
transferirse después sin fuga, doble destrucción ni pérdida del valor anterior del destino.

### 5.11 Cerrar `NSC-LIMIT-017`: inicializadores de arrays con longitud inferida

**Estado 0.8.0-alpha:** cerrado para arrays nativos de valores. El parser acepta la longitud
omitida, HIR conserva conversiones y orden, y LLVM inicializa longitud y elementos una sola vez.
La fixture `c50` cubre primitivos, structs, coma final, longitud y orden O0/O2.

La actualización todavía rechaza la forma:

```csharp
Map[] maps = new Map[] { first, second, third };
```

El workaround soportado es una expansión puramente mecánica:

```csharp
Map[] maps = new Map[3];
maps[0] = first;
maps[1] = second;
maps[2] = third;
```

Para tablas grandes, la conversión debe conservar el número exacto de elementos, índices
continuos desde cero, cada literal sin modificación y el orden original. Los datos del consumidor
no forman parte del compilador; el repro propio de `nsc` sólo necesita aislar la construcción y
demostrar equivalencia.

La implementación del lenguaje requiere:

- parsear `new T[] { ... }` e inferir la longitud a partir del número de elementos;
- evaluar cada expresión una sola vez y de izquierda a derecha;
- aplicar las mismas conversiones de elemento que una asignación ordinaria al array;
- definir el inicializador vacío y diagnosticar elementos incompatibles en su posición exacta;
- conservar ownership y ejecutar cleanup correcto si falla una inicialización parcial;
- cubrir valores primitivos, structs, referencias, elementos `owned` y arrays anidados;
- validar HIR, MIR, LLVM, PE y ejecución O0/O2.

La materialización como datos constantes o cualquier optimización de tablas es posterior y no
forma parte de este límite. Primero debe existir semántica correcta equivalente a `array[n]` más
asignaciones por índice.

**Cierre:** la forma compacta y su expansión producen los mismos valores en el mismo orden, con la
misma destrucción, y una fixture propia impide la regresión.

### 5.12 Cerrar `NSC-LIMIT-018`: especialización genérica por miembro alcanzado

**Estado 0.8.0-alpha:** cerrado. Las firmas se materializan para lookup, mientras que un método
ordinario especializado sólo se analiza y emite si una llamada o conversión a delegate lo exige.
Constructores, destructores y virtuales permanecen eager por lifetime/despacho. `List<Pair>` usa
`Add`, `Count` e indexador sin exigir `<`; llamar realmente el miembro que ordena vuelve a activar
el diagnóstico.

Crear `List<Pair>` y usar únicamente `Add` no debe fallar porque `Sort` o `BinarySearch`
contengan `<` y `Pair` no tenga orden natural. El modelo actual clona y valida todos los miembros
de la clase genérica al especializar el tipo, incluso los que el programa nunca usa.

La solución puede materializar/validar por miembro alcanzado o expresar constraints precisos en
los miembros que requieren orden. En ambos casos debe conservar la expectativa de C#: instanciar
una colección no obliga a que `T` satisfaga operaciones utilizadas exclusivamente por métodos no
invocados.

Las regresiones deben cubrir tipo sin `<`, uso de almacenamiento/indexador, llamada posterior a
`Sort` con diagnóstico en ese sitio, overloads, herencia y especializaciones cross-project.

**Cierre:** `List<Pair>` con `Add` compila; invocar una operación que realmente exige orden
funciona sólo con un `T` válido o falla con el constraint preciso.

### 5.13 Cerrar `NSC-LIMIT-019`: plegado transitivo de campos `const`

**Estado 0.7.6:** cerrado. Todos los inicializadores se enlazan antes de evaluar dependencias;
un conjunto de visita detecta ciclos reales en vez de usar el ordinal alfabético del miembro como
aproximación del orden. Pasan referencia adelantada, calificada, transitiva y cross-project, y el
ciclo directo/indirecto conserva rechazo.

Una referencia calificada como `ConsumerLimits.Alias = SourceLimits.Source` debe ser una
constante cuando el campo origen también lo sea. Se requiere plegado transitivo entre tipos y
proyectos, conversiones constantes normales, memoización y detección de ciclos con diagnóstico
accionable.

La semántica debe seguir la expectativa de C#: el consumidor observa el valor constante, no una
lectura estática en runtime. Las fixtures deben cubrir cadenas, casts, enums, accesibilidad,
cross-project y ciclos directos e indirectos.

### 5.14 Cerrar `NSC-LIMIT-020`: parámetros con valor predeterminado

**Estado 0.8.0-alpha:** cerrado en el grafo fuente. Métodos y constructores aceptan defaults
literales, overload resolution prefiere menos argumentos predeterminados y HIR materializa los
omitidos con el tipo formal. La serialización de defaults en metadata `.lib` no se declara cerrada.

El parser debe aceptar firmas como `void Add(float amount = 1.0f)`, conservar la expresión en AST
y metadata, considerarla durante overload resolution y materializar el argumento omitido también
a través de `ProjectReference`.

Para mantener compatibilidad dirigida con C#, los defaults deben restringirse a expresiones
constantes admitidas, respetar el orden obligatorio/opcional de parámetros y comportarse como
valor fijado en el contrato de la llamada. Se requieren casos omitido/explícito, varios opcionales,
overloads, métodos genéricos y consumo cross-project.

### 5.15 Cerrar `NSC-LIMIT-021`: préstamo de temporales `owned string`

**Estado 0.8.0-alpha:** cerrado. Un temporal fresco se guarda en un owner oculto durante toda la
expresión; la llamada lo recibe prestado y el cleanup lo libera exactamente una vez tanto en el
camino normal como en unwind. Parámetros de usuario sin `borrowed` continúan rechazándose si
podrían retener la referencia.

El diagnóstico preciso reemplaza la clasificación provisional anterior: `"a" + "b"` sí se tipa
correctamente como un string nuevo y `owned`. El fallo aparece al prestar ese temporal a un
parámetro string sin calificador o `borrowed`:

```csharp
raii string copy = String.Copy("a" + "b");
```

La llamada se rechaza antes de HIR porque el argumento lleva
`ownershipTransfer=true` y el parámetro seleccionado no es `owned`. No es un defecto de
`String.Copy`, binding o linker; falta una regla general de lifetime de expresión.

El workaround localizado materializa primero un owner con nombre:

```csharp
raii string temporary = String.Concat("a", "b");
raii string copy = String.Copy(temporary);
```

Para shaders y otros textos embebidos, esa transformación debe conservar longitud, saltos de línea
y bytes exactamente iguales. El workaround no cambia la regla futura.

El soporte requerido es:

- crear un owner RAII oculto para todo temporal fresco que se preste a un parámetro no retenedor;
- mantenerlo vivo hasta terminar la expresión completa y destruirlo exactamente una vez;
- admitir parámetros `borrowed string` y parámetros Core sin calificador cuyo contrato sea no
  retenedor;
- conservar evaluación de izquierda a derecha con varios temporales y llamadas anidadas;
- ejecutar cleanup correcto si un argumento posterior o la propia llamada lanza una excepción;
- impedir que el préstamo escape o llegue a una API que pueda conservar la referencia;
- implementar la regla para temporales `owned` en general, usando string como caso contractual,
  no como excepción cableada a `String.Copy`;
- validar diagnóstico negativo, HIR, MIR, LLVM y ejecución O0 antes de habilitar O2.

**Cierre:** `String.Copy("a" + "b")` compila y devuelve el contenido completo; el owner oculto
vive durante la llamada y se destruye una vez. Las pruebas propias de `nsc` son el gate. La
retirada de locales escalonadas en consumidores externos corresponde a esos proyectos.

### 5.16 Consolidar reglas transversales

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

### 5.17 Cerrar decisiones semánticas que todavía permiten una respuesta silenciosa

Antes de llamar completo al lenguaje existente hay que inventariar todos los límites documentados
y clasificarlos como bloqueo de 1.0, rechazo deliberado o feature futura. Ninguno puede quedar como
«compila, pero nadie sabe quién responde por el resultado».

NSC-LIMIT-021 define la respuesta para un owner recién creado enviado a un parámetro
inequívocamente no retenedor:

```csharp
Consumir(new Recurso()); // ¿quién destruye Recurso si el parámetro no es owned?
```

El compilador materializa un owner temporal, lo presta durante la expresión completa y lo destruye
después. La decisión que aún debe cerrarse es si todo parámetro sin calificador garantiza que no
retiene la referencia; si no lo garantiza, sólo un contrato `borrowed` explícito puede habilitar
esa regla. No se permite inferir silenciosamente un lifetime más largo que el escrito en la firma.

También deben recibir una decisión explícita:

- inicialización estática y su orden, o su exclusión de 1.0;
- propiedades sólo `set`, como feature soportada o rechazo sintáctico preciso;
- plegado de `const` locales derivados, sin dos evaluadores con reglas distintas;
- comportamiento de temporales owner que salen por `return`, llamadas y constructores;
- límites de `volatile`, dejando claro que no es sincronización;
- cualquier operación que hoy llegue al backend para descubrir que era inválida.

No todas tienen que implementarse. Todas tienen que terminar en una de tres respuestas comprobables:
funciona bajo un contrato, se rechaza con diagnóstico, o está declarada fuera de 1.0.

### 5.18 Gate de salida 0.7

- cero P0 y P1 conocidos en el lenguaje implementado;
- cero miscompile conocido;
- todas las fixtures positivas pasan en O0 y O2;
- todos los negativos fallan por el diagnóstico esperado;
- ownership no produce doble destrucción, fuga conocida aceptada ni use-after-move silencioso;
- el verificador de LLVM acepta todo módulo emitido;
- cada bug corregido tiene rojo anterior reproducido y verde posterior;
- APIs genéricas públicas de una biblioteca aceptan tipos válidos definidos sólo por el consumidor;
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

### 6.6 Veracidad de paquetes y bindings

Los reportes de bindings pertenecen a `nsc-bindings-devel`, `nsbind`, `nsb` o `nspkg`
según el caso; no deben corregirse dentro del frontend o backend del compilador. Aun así, un
toolchain 1.0 no puede publicar una biblioteca como lista cuando sólo comprobó que existe un
archivo.

Para cada paquete oficial que se declare soportado:

- `nsb doctor` debe exigir artifacts reales y nunca informar «LISTO» con un inventario `0/0`;
- el manifest declara capacidades, subsistemas, features habilitadas/deshabilitadas, baseline de
  CPU y dependencias runtime;
- `nspkg` resuelve dependencias transitivas de `ProjectReference` y usos mediante alias;
- los archives publicados se prueban combinados para detectar símbolos globales duplicados;
- el DLL/archivo realmente distribuido se consulta mediante fixtures de sus capacidades;
- una máquina limpia recibe o diagnostica todas las dependencias necesarias.

Esto no convierte la amplitud máxima de los 21 bindings auditados en gate del lenguaje. Un paquete
puede ser parcial si su nombre, manifest y documentación describen con precisión esa superficie.
Lo inadmisible es confundir presencia con completitud.

La evidencia externa deja cuatro acciones concretas para los repositorios responsables:

- `BIND-LINK-001`: hacer componibles los archives STB/MP3 y probar el co-link publicado;
- `BIND-009`: no publicar Assimp reducido como si incluyera FBX y el resto de importers;
- `PKG-011`: detectar paquetes utilizados únicamente mediante `using Alias = ...`;
- `nsb doctor`: corregir el falso «LISTO» cuando encontró `0/0` artifacts.

`PKG-008` ya está corregido. Las superficies BIND-003 a BIND-007 continúan en el backlog separado
de bindings. Su avance no forma parte del gate del compilador 1.0.

### 6.7 Gate de salida 0.8

Un repositorio nuevo, fuera de `A:\nsc-devel`, puede:

1. declarar un proyecto;
2. enlazar una biblioteca C externa;
3. producir Debug y Release;
4. ejecutarse en O0 y O2;
5. depurarse con símbolos;
6. reproducir el binario;
7. hacerlo siguiendo documentación pública, sin scripts privados.

---

## 7. Gate 0.85 — ABI de plataforma, no catálogo de bindings

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

### 7.4 Los bindings no son gate del compilador

La expansión del catálogo vive en el roadmap y en los repositorios de bindings. Native Sharp 1.0
sólo exige que el compilador pueda expresar y enlazar correctamente las ABI nativas que declara
soportar. La cobertura concreta de cada paquete continúa evolucionando por separado.

Prioridades transversales del ecosistema:

- evitar DLL reducidos publicados bajo un nombre que sugiera soporte upstream completo;
- generar superficies desde headers/exports con exclusiones pequeñas y auditables;
- mantener accessors C estables para tipos C++ opacos o bitfields difíciles de representar;
- resolver composición de archives y dependencias runtime en el paquete, no en cada consumidor;
- probar formatos y subsistemas reales, no sólo carga del DLL.

No se agrega ABI C++ general al lenguaje para compensar una fachada incompleta. Cuando un shim C es
la frontera correcta, el trabajo pertenece al binding.

### 7.5 `CORE-LUNAR-003` / `BIND-LUNAR-002`: fecha y hora mínima de plataforma

`NativeSharp.Core.DateTime` expone actualmente ticks UTC, pero todavía no ofrece descomposición de
calendario ni un equivalente de `ToLocalTime`. Esto no es un defecto del compilador. Mientras Core
no tenga ese contrato, un consumidor Windows puede convertir mediante la API horaria nativa y
formatear explícitamente el resultado.

El workaround contractual conserva exactamente:

```text
yyyy-MM-dd HH:mm
```

La implementación mínima de Core debe incluir:

- conversión entre ticks UTC y componentes de calendario;
- conversión UTC/local delegada a las reglas de zona horaria y horario de verano del sistema;
- componentes `year`, `month`, `day`, `hour` y `minute` sin asignaciones ocultas;
- formato invariante y explícito para patrones numéricos pequeños;
- pruebas de años bisiestos, cambios de día/año y transiciones de horario de verano;
- errores claros para valores fuera del rango soportado.

No se requiere un framework de culturas, calendarios empresariales, parsing libre, zonas horarias
embebidas ni integración con SQL. Si una aplicación necesita más, puede usar un binding externo.

**Retiro del workaround:** cuando Core produzca el mismo calendario local y el formato
`yyyy-MM-dd HH:mm` mediante una API estable, el consumidor puede eliminar su llamada Win32 sin que
ese proyecto se convierta en gate del runtime.

### 7.6 Gate de salida 0.85

Fixtures mínimas y autocontenidas demuestran structs, enums, callbacks, function pointers, strings,
buffers, handles y vtables en ambas direcciones. El gate mide las capacidades ABI del compilador;
no exige un consumidor externo ni declara completo ningún catálogo externo.

---

## 8. Gate 0.9 — Frontera de bibliotecas precompiladas

`0.9` demuestra que una unidad NativeSharp es un producto binario reutilizable, no sólo un grupo de
fuentes que el ejecutable recompila. Todo el gate pertenece al compilador, metadata y enlazador.

### 8.1 Producto publicable

Un productor y un consumidor separados deben funcionar sin compartir fuentes. El producto tiene
una forma inequívoca:

- biblioteca estática COFF: `.lib`, header C y `.nsmeta`;
- biblioteca dinámica: `.dll`, import `.lib`, header C y `.nsmeta`;
- identidad de módulo, target, versión de formato y huella ABI verificables;
- exports e imports explícitos;
- error temprano cuando binario, metadata o declaración consumidora discrepan.

La ABI C permanece como frontera mínima y estable. El consumo NativeSharp de 0.9 materializa desde
`.nsmeta` tipos de valor, clases opacas, miembros concretos, defaults, ownership y callbacks crudos
sin redeclaraciones manuales.

### 8.2 Composición

- varias bibliotecas pueden participar en un mismo enlace sin colisiones de identidad;
- una estática consolida sus dependencias estáticas, pero deja los imports del proceso al enlace final;
- una DLL conserva y declara correctamente sus dependencias dinámicas;
- las dependencias transitivas se validan por identidad y huella;
- un cambio incompatible señala módulo y símbolo responsables antes de producir el binario final;
- los auxiliares internos del runtime y RTTI no escapan como símbolos fuertes duplicados.

### 8.3 Matriz contractual

La certificación cubre productores y consumidores separados para estática y DLL, en O0/O2 y con
los dos caminos de enlace soportados. Debe ejercer:

- funciones, overloads y defaults;
- structs, enums, clases opacas y delegates;
- ownership en parámetros, retornos y cleanup;
- especializaciones genéricas ya cerradas en el productor conservadas en metadata;
- métodos virtuales e interfaces preservados en metadata, sin degradarlos a llamadas directas;
- múltiples bibliotecas y dependencias transitivas;
- metadata anterior compatible y metadata futura/incompatible rechazada;
- ejecución real del consumidor, no sólo existencia de archivos.

### 8.4 Congelación 0.9

Al cerrar el gate quedan congelados el mangling público, ABI del runtime, formato `.nsproj`, formato
`.nsmeta`, reglas de compatibilidad y CLI de producción/consumo de bibliotecas. Desde ese punto,
todo cambio incompatible exige migración documentada y aumento de versión.

---

## 9. Gate 1.0 — Contrato público

### 9.1 Lenguaje

**Estado E7 al 2026-09-07:** instalado, `LANDED` y certificado por G00. La autoridad normativa,
gramática y mapa de conformidad están en `spec/`; la historia técnica está en 140 y el acta de
promoción está en 141.

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

**Estado E8 al 2026-09-07:** primera megaentrega instalada y certificada en Stage 61. Ya están
implementados el presupuesto de diagnósticos, la frontera de fallos internos, la verificación del
LLVM cargado y el contrato trazable del compilador. Las matrices locales y G00 están verdes; el
baseline vigente es `20260907-170412-16b5b8cb`.

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

**Estado E9 al 2026-09-07:** Stage 62, Stage 63, Stage 64 y Stage 66 están certificados. Además del
contrato, layout e inventario previos,
el gate construye e instala dos árboles limpios, compara `NSINSTALL 1`, fija únicamente la identidad
del build para exigir igualdad completa de ambos `nsc.exe` y registra `NSREPRO 1`. La instalación
genera `NSUPDATE 1`, comprueba los manifiestos anterior y siguiente, prevalida todo retiro y sólo
elimina archivos que continúan iguales a los bytes cuya propiedad estaba registrada. El detalle de
implementación está en 150 y la promoción está en 151.

- contrato determinista de paquete: implementado y certificado en Stage 62;
- versiones compatibles de compilador, lenguaje, LLVM, NSMETA y ABI: implementadas y verificables;
- layout mínimo instalado autoverificable: implementado y certificado en Stage 63;
- inventario criptográfico completo: implementado y certificado en Stage 64;
- empaquetado reproducible completo: implementado y certificado en Stage 66;
- compilación desde una máquina limpia: documentada con el toolchain vigente en Stage 64;
- PDB y depuración funcional;
- `.lib` externas y proyectos múltiples;
- builds Debug/Release;
- logs suficientes para reproducir un fallo;
- actualización y retiro sin apropiarse de archivos vecinos: implementados y certificados en Stage 66.

### 9.4 Compatibilidad

**Estado E10 al 2026-09-07:** implementada y certificada en Stage 66.
`NativeSharpCompatibility.policy` publica por separado las versiones de compilador, lenguaje,
NSMETA, Native ABI, Runtime ABI y formatos del paquete. El propio driver puede imprimirla,
escribirla, verificarla y decidir un requisito canónico `major.minor.patch`. La política se instala,
forma parte del inventario criptográfico y no contiene la identidad efímera de construcción.

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
5. Corpus del compilador ejecutado durante un soak reproducible y prolongado.
6. Cero fuga o doble destrucción conocida en el corpus.
7. Binarios reproducibles.
8. Manual del lenguaje, guía de ownership, guía de FFI y guía de distribución del toolchain.
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
- una matriz no cuenta si sus ejecutables aparecen, desaparecen o cambian durante la corrida;
- compilar no sustituye ejecutar;
- ejecutar correctamente no sustituye comprobar destrucción, llamadas o forma del IR cuando esos
  son parte del contrato;
- compartir implementación no sustituye probar caminos semánticamente distintos;
- no se agregan features por comparación de checklist con otro lenguaje.

---

## 12. Orden recomendado

```text
0.7.4  Build instalado; el primer G00 detectó orden de medición inválido
0.7.5  Runner G00 corregido; baseline LANDED/certificado; NSC-LIMIT-011 cerrado
0.7.6  Entrega grande: NSC-LIMIT-012/013/015/019 + NSC-LUNAR-007 cerrados
0.8.0-alpha  Entrega concentrada certificada; NSC-LIMIT-005/016/017/018/020/021 cerrados
0.9.0        Cierre local: DLL/import `.lib`, estática, consumo precompilado y NSMETA 1.5
0.9.0+G00    Baseline instalado y certificado — completado el 2026-09-07
1.0-E5/E6    Diagnósticos ABI y verificación física de bibliotecas — instaladas y certificadas
1.0-E7       Especificación, gramática y trazabilidad normativa — instalada y certificada
1.0-E8       Endurecimiento del compilador — Stage 61 instalado y certificado por G00
1.0-E9       Contrato, instalación e inventario — Stage 62/63/64 instalados y certificados
1.0-E10      Reproducibilidad, ciclo de paquete y compatibilidad — Stage 66 certificado
1.0-E11      Clausuras léxicas compartidas y lambdas tipadas — Stage 67 certificado
1.0.0-rc.1  instalación externa y periodo de estabilización
1.0.0  contrato público
```

El orden puede cambiar por un bug bloqueante o por evidencia del corpus del compilador. No cambia por el
deseo de añadir una feature llamativa.

---

## 13. Definición de «1.0 Glory»

La gloria no es que Native Sharp tenga más palabras clave que C++, más biblioteca que C# o más
plataformas que Java.

La gloria es ésta:

> Un programador abre un proyecto vacío, escribe software nativo con sintaxis clara, ve y controla
> sus costos, llama directamente a la plataforma, depura código fuente real, obtiene el mismo
> comportamiento en Debug y Release, y puede confiar en que ownership, ABI y optimización no van a
> traicionarlo.

Si Native Sharp consigue eso sin GC, sin reflection dinámica, sin runtime pesado y sin convertirse
en una plataforma web/SQL/enterprise, entonces 1.0 habrá cumplido exactamente la promesa correcta.
