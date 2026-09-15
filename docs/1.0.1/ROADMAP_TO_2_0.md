# NativeSharp: roadmap vigente de 1.0.1-dev hacia 2.0

**Edición / Edition:** 1.0.1

**Canal / Channel:** `1.0.1-dev`

**Snapshot inicial / Initial snapshot:** `nativesharp-1.0.1-dev-000`

**Commit del snapshot / Snapshot commit:** `5ff40d8ab53d3af9ade6314c9c3d55df761f18e8`

**Fecha de corte / Snapshot date:** 2026-09-12

**Target contractual actual / Current contractual target:** `x86_64-pc-windows-msvc`

## 1. Propósito y autoridad / Purpose and authority

Este documento gobierna la evolución de NativeSharp desde la línea activa `1.0.1-dev` hasta una
eventual versión `2.0`. Sustituye como guía de ejecución a los roadmaps históricos anteriores a
1.0, pero no borra su evidencia. La especificación del lenguaje, el contrato del compilador y el
contrato de depuración conservan autoridad normativa sobre sus respectivas superficies.

This document governs NativeSharp evolution from the active `1.0.1-dev` line toward an eventual
`2.0` release. It supersedes pre-1.0 historical roadmaps as an execution guide without erasing
their evidence. The language specification, compiler contract, and debug artifact contract remain
normative for their respective surfaces.

Las autoridades vigentes son:

- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`;
- `docs/1.0.1/language/NativeSharp_Grammar_1_0_1.ebnf`;
- `docs/1.0.1/language/NativeSharp_1_0_1_conformance.map`;
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`;
- `docs/1.0.1/debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md`;
- `DLL_HANDOVER_AND_ROADMAP.md` para bibliotecas precompiladas;
- `PDB_HANDOVER_AND_ROADMAP.md` para CodeView/PDB.

Cuando un documento histórico contradiga esta edición, se usa este documento para prioridad y
se usa el contrato normativo correspondiente para semántica. Ninguna tabla de roadmap cambia por
sí sola el lenguaje.

## 2. Norte del producto / Product direction

NativeSharp es un lenguaje y toolchain nativo AOT para construir aplicaciones, juegos,
herramientas y bibliotecas sin CLR, GC implícito ni runtime administrado. Su sintaxis familiar no
lo convierte en una implementación reducida de C#.

NativeSharp is a native AOT language and toolchain for applications, games, tools, and libraries
without the CLR, an implicit garbage collector, or a managed runtime. Familiar syntax does not
make it a reduced C# implementation.

La meta hacia 2.0 es que un programador pueda:

1. expresar ownership, borrowing, RAII, layout y ABI sin ambigüedad;
2. producir EXE, bibliotecas estáticas y DLL reutilizables sin compartir fuentes;
3. consumir APIs C y COM con una frontera nativa auditable;
4. depurar código NativeSharp mediante CodeView y PDB estándar;
5. obtener diagnósticos tempranos en lugar de metadata, IR u objetos inutilizables;
6. reproducir, instalar, verificar y actualizar el toolchain fuera del árbol de desarrollo;
7. evolucionar dentro de 1.x sin romper contratos congelados accidentalmente;
8. reservar 2.0 para cambios deliberadamente incompatibles y acompañados de migración.

## 3. Lo que 2.0 no significa / What 2.0 does not mean

La versión 2.0 no será una lista de características copiadas de otro lenguaje. En particular, este
roadmap no incorpora automáticamente:

- CLR, IL, JIT o runtime administrado;
- garbage collector o tracing global;
- reflection dinámica, `dynamic`, boxing general o ejecución dinámica de código;
- LINQ, expression trees, framework web, ORM o infraestructura enterprise;
- un engine, ECS, scene graph, renderer o asset pipeline dentro de Core;
- compatibilidad ABI general con clases C++;
- soporte multiplataforma nominal sin un target y una matriz reales;
- `params`, argumentos nombrados, métodos de extensión, interpolación, `??`, `??=` o `?.` por
  simple paridad con C#;
- self-hosting como requisito de versión.

Una comodidad puede entrar si existe un consumidor, tiene semántica nativa pequeña, costo
predecible y una prueba clara. Su existencia en C# no constituye evidencia suficiente.

## 4. Punto de partida real: 1.0.1-dev-000 / Actual starting point

El snapshot `nativesharp-1.0.1-dev-000` congela el primer punto de esta serie. Es un marcador de
historia, no una certificación nueva y no incluye archivos sin commit.

### 4.1 Lenguaje y ABI implementados

- ownership `owned`, `borrowed`, `move`, `raii` y destrucción determinista;
- clases, structs, interfaces, herencia, virtuales, propiedades, indexadores y operadores;
- genéricos AOT, constraints y especializaciones cerradas;
- delegates, eventos, lambdas, closures y barrera de excepciones C;
- excepciones nativas, `finally`, cleanup y SEH;
- punteros, unions, arrays fijos, `ref/out/in`, handles y ABI Microsoft x64;
- `extern C`, imports DLL, exports C y llamadas por vtable expresables;
- `const string`, préstamos de string en constructores y Core nativo.

### 4.2 Toolchain implementado

- emisión de OBJ, EXE, `.lib`, DLL, import library, headers y NSMETA;
- consumo por `ProjectReference` y `LibraryReference`;
- HIR/MIR 1.7 con promoción conservadora de clases `raii` no escapantes y coordenadas ejecutables
  de bitfields;
- structs y unions anónimos inline con carrier explícito opcional, promoción recursiva y layout
  anidado determinista;
- sintaxis, layout y acceso MSVC de bitfields nombrados, sin nombre y de ancho cero, incluidos
  cambios de unidad, barreras, `pack`, structs, clases, unions, máscaras de lectura/escritura y
  extensión de signo; las variantes `volatile` acceden a la unidad completa sin atomics ni fences;
- NSMETA 1.18 con identidad física, accesibilidad efectiva, templates, clausura transitiva,
  cuerpos inline reutilizables, campos volatile, retornos `ref`, contratos `noheap` y carriers
  anónimos y coordenadas físicas de bitfields preservados;
- verificación cruzada de metadata, COFF, import library y exportaciones PE;
- identidad de build, instalación verificable, inventario criptográfico y actualización segura;
- documentación 1.0.1 autocontenida e instalable.

### 4.3 Depuración implementada en el productor

- perfiles `-g0`, `-gline-tables-only` y `-g` independientes de optimización;
- CodeView en COFF y PDB/MSF estándar mediante la ruta soportada;
- archivos, líneas, parámetros, locales, `this`, tipos y scopes;
- funciones sintetizadas, thunks ABI, interfaces, SEH y closures;
- checksums SHA-256 y `-fdebug-prefix-map` para identidad reproducible;
- representación honesta de valores disponibles o eliminados bajo optimización.

### 4.4 Distinción de estado

`Implementado` no significa automáticamente `certificado por el baseline más reciente`. Todo build
continúa mostrando `1.0.1-dev+build.<UTC>.<id>` hasta una promoción explícita. El roadmap nunca
inferirá certificación a partir de una compilación aislada, de un archivo generado o de la mera
existencia de una prueba.

## 5. Política de snapshots de desarrollo / Development snapshot policy

Los hitos locales usan nombres con ancho fijo:

```text
nativesharp-1.0.1-dev-000
nativesharp-1.0.1-dev-001
nativesharp-1.0.1-dev-002
...
```

Reglas:

1. cada snapshot apunta a un commit existente de `development`;
2. no se cambia de rama para crearlo y no se desarrolla encima de él;
3. no incorpora modificaciones sin commit;
4. una vez creado, su punta no se mueve;
5. se crea al cerrar un lote coherente, no por cada microcambio;
6. crear el snapshot no compila, instala ni ejecuta baseline;
7. publicación al remoto y eliminación son decisiones explícitas separadas;
8. `dev-NNN` ordena hitos, pero no sustituye SemVer ni la identidad del build;
9. los release candidates usan una identidad de release, no reciclan un snapshot de desarrollo;
10. un baseline sólo se ejecuta cuando se solicita expresamente.

The fixed-width sequence provides readable local milestones while `development` keeps moving.
Snapshots are immutable by convention, contain committed state only, and never imply certification.

## 6. Criterio para aceptar trabajo / Work admission rule

Una tarea entra al roadmap activo cuando satisface estas preguntas:

1. **Consumidor:** ¿qué programa, binding, biblioteca, debugger o herramienta lo necesita?
2. **Contrato:** ¿qué regla vigente se incumple o qué contrato nuevo se propone?
3. **Natividad:** ¿puede resolverse sin introducir un subsistema administrado?
4. **Costo:** ¿qué memoria, código, metadata o runtime agrega?
5. **Evidencia:** ¿cuál es el repro mínimo o el resultado observable?
6. **Compatibilidad:** ¿cabe en 1.x o exige deliberadamente 2.0?

Los reportes externos se reducen a una propiedad del compilador. No se modifica un binding para
ocultar un defecto del lenguaje, metadata o ABI. Tampoco se atribuye al compilador una carencia que
pertenezca al port, al paquete o a una decisión explícita del lenguaje nativo.

## 7. Camino inmediato: cerrar 1.0.1 / Immediate path: close 1.0.1

### 7.1 R1 — Defectos dirigidos por consumidores

**Prioridad:** P0.

**Objetivo:** permitir que bindings y aplicaciones reales consuman el compilador instalado sin
deformar su superficie pública.

Proceso por defecto:

1. recibir el error exacto y el build que lo produjo;
2. reducirlo dentro de `A:\nsc-devel` cuando sea necesario;
3. corregir una única fuente de verdad;
4. crear prueba dirigida al defecto;
5. compilar únicamente el componente o el compilador requerido;
6. instalar el build solicitado para validación externa;
7. guardar un commit local y su Markdown bilingüe.

Durante este carril no se ejecutan bindings completos ni baseline salvo solicitud explícita. La
iteración ordinaria usa O0. O2 y matrices amplias pertenecen a una certificación deliberada.

**Salida:** cero P0 reproducible conocido en los consumidores activos y ningún workaround que
contradiga la API canónica.

### 7.2 R2 — Frontera precompilada completa

El trabajo DLL/NSMETA P1–P6 está cerrado para el corpus actual. Antes de promover 1.0.1 deben
cerrarse o clasificarse explícitamente:

- PDB asociado a bibliotecas y DLL;
- consumo desde hosts externos mediante la ABI pública;
- verificación final de productor/consumidor sin fuentes compartidas;
- compatibilidad N−1/N para la versión de NSMETA realmente publicada;
- composición de múltiples bibliotecas sin símbolos auxiliares fuertes duplicados;
- fallos tempranos para identidad física, clausura genérica y dependencias transitivas.

**Salida:** un productor publica `.lib/.dll/.nsmeta/.h`; un consumidor limpio compila, enlaza y
ejecuta usando sólo los artefactos declarados.

### 7.3 R3 — Cierre del productor PDB

El productor ya cubre PDB-P1–P6. El cierre 1.0.1 requiere:

- fixture entregable y estable para el debugger;
- validación GUID/age, checksum y remapeo de fuente;
- stack físico a través de callbacks, interfaces, excepciones y closures;
- parámetros, locales y tipos representativos;
- PDB para bibliotecas cuando el producto lo solicite;
- clasificación inequívoca de fallos entre NSC, linker y consumidor.

El debugger continúa siendo de sólo lectura desde este repositorio. NSLink conserva su propio
dueño; NSC debe entregar objetos correctos y documentar el handoff, no implementar el linker.

**Salida:** el consumidor puede colocar breakpoint, resolver archivo/línea, inspeccionar una local
y recorrer un stack representativo sin formato privado.

### 7.4 R4 — Coherencia documental y del producto instalado

- la documentación 1.0.1 describe la implementación actual y no estados históricos;
- `nsc --version`, contrato, manifest, NSMETA y ABI muestran identidades compatibles;
- la instalación incluye la edición correcta bajo `share/nativesharp/docs/1.0.1`;
- los ejemplos y tutoriales pueden evolucionar en su carril sin convertirse en autoridad del
  compilador;
- un reporte conserva ruta del ejecutable, versión completa, hash/build y configuración.

### 7.5 R5 — Promoción de 1.0.1

La promoción sólo ocurre por instrucción explícita. Su gate mínimo es:

1. árbol de fuentes y commits identificados;
2. compilación limpia del producto;
3. pruebas dirigidas de las áreas modificadas;
4. baseline acordado, ejecutado una sola vez para el lote;
5. instalación verificable y documentación coincidente;
6. lista de limitaciones aceptadas que no contradiga la promesa;
7. snapshot o tag de release inmutable.

Hasta entonces, la versión permanece `1.0.1-dev` aunque un consumidor específico pase.

## 8. Línea 1.x: madurez compatible / The compatible 1.x line

La serie 1.x debe mejorar el producto sin romper sus contratos públicos. Sus prioridades son:

### 8.1 Correctness y diagnósticos

- eliminar miscompiles antes de ampliar superficie;
- unificar decisiones semánticas que hoy aparezcan en varias etapas;
- producir archivo, línea, columna y causa cuando exista ubicación fuente;
- rechazar metadata, IR o COFF inutilizable en el productor;
- conservar equivalencia semántica entre configuraciones soportadas.

### 8.2 Runtime y Core nativos

- versionar explícitamente el ABI del runtime;
- completar únicamente APIs nativas justificadas por consumidores;
- mantener ownership y destrucción visibles;
- evitar servicios globales que requieran finalización ficticia;
- separar biblioteca conveniente de primitiva obligatoria del lenguaje.

### 8.3 Toolchain cotidiano

- configuraciones Debug/Release reproducibles;
- bibliotecas externas y rutas de búsqueda deterministas;
- diagnósticos de arquitectura, símbolo y formato incompatibles;
- instalación, actualización, limpieza y desinstalación seguras;
- paquetes que demuestren artefactos y capacidades reales.

### 8.4 Ecosistema sin inflar Core

Bindings de gráficos, audio, ventanas, assets y plataforma son consumidores. Sus necesidades pueden
demostrar huecos del compilador, pero su catálogo no se incorpora a Core ni se usa como checklist
de características de lenguaje.

### 8.5 Compatibilidad 1.x

Dentro de 1.x son admisibles:

- correcciones de bugs y diagnósticos;
- metadata aditiva legible por consumidores compatibles;
- nuevas APIs que no cambien layout o semántica existente;
- optimizaciones que preserven conducta observable;
- nuevos gates y verificadores.

Un error que cause corrupción, miscompile o metadata inutilizable puede corregirse aunque algún
programa dependiera accidentalmente de él. La ruptura debe documentarse como bug fix, no ocultarse.

## 9. Puerta de entrada a 2.0 / Entry gate for 2.0

NativeSharp no cambia de major porque el roadmap haya acumulado tareas. Se abre 2.0 sólo cuando
exista al menos un cambio necesario que no pueda entregarse honestamente dentro de 1.x.

### 9.1 Cambios que pueden justificar 2.0

- modificación incompatible del Native ABI o Runtime ABI;
- nuevo mangling público incompatible;
- cambio incompatible del modelo de ownership, borrowing o destrucción;
- rediseño incompatible de excepciones o calling conventions;
- formato NSMETA que no pueda conservar lectura o migración 1.x;
- cambio incompatible de `.nsproj` o del contrato de paquetes;
- redefinición de tipos fundamentales, layout, ancho o semántica numérica;
- cambio de target que obligue a generalizar reglas actualmente fijadas a Windows x64.

### 9.2 Cambios que no justifican 2.0 por sí solos

- agregar una API o diagnóstico compatible;
- corregir un crash o miscompile;
- ampliar PDB sin cambiar semántica del programa;
- soportar otro binding;
- acelerar compilación o enlace;
- agregar una comodidad sintáctica con lowering compatible;
- crear más documentación, ejemplos o paquetes.

### 9.3 Libro mayor de rupturas / Breaking-change ledger

La distribución 1.0.1-dev materializa el punto de comparación en
`share/nativesharp/toolchain/NativeSharp2Migration.baseline` (`NSMIGRATION 1`). El artefacto no
abre 2.0: congela las versiones vigentes de metadata, MIR y ABI que toda propuesta deberá
clasificar. `nsc --verify-migration-baseline` permite comprobar que una copia corresponde
exactamente al compilador que la evalúa.

The 1.0.1-dev distribution materializes the comparison point as
`share/nativesharp/toolchain/NativeSharp2Migration.baseline` (`NSMIGRATION 1`). It does not open
2.0; it freezes the current metadata, MIR, and ABI identities that every proposal must classify.

Antes de escribir código 2.0 se crea un registro por propuesta con:

1. contrato 1.x afectado;
2. consumidor que demuestra la necesidad;
3. alternativas compatibles descartadas;
4. nueva semántica o formato;
5. impacto en fuentes, binarios, metadata y herramientas;
6. estrategia de diagnóstico y migración;
7. versión mínima de cada componente;
8. pruebas de compatibilidad y rechazo.

Sin ese registro, una tarea permanece en 1.x o fuera del roadmap.

## 10. Programa de ingeniería 2.0 / 2.0 engineering program

Las siguientes fases son dependencias, no promesas de agregar todas las características listadas.
Cada una se abre sólo con contratos y consumidores concretos.

### 2.0-A — Contratos semánticos consolidados

**Objetivo:** ninguna semántica importante depende accidentalmente de LLVM o de una optimización.

- contrato único de conversiones, narrowing y overflow;
- modelo de memoria, atomics, fences y acceso observable;
- folding de constantes bajo las mismas reglas del runtime;
- ownership, escapes y destrucción representados antes del backend;
- diagnósticos de invariantes con ubicación fuente;
- diferencias de target declaradas y versionadas.

**Gate:** fixtures semánticas demuestran la misma conducta en configuraciones soportadas; el IR se
inspecciona sólo cuando su forma forma parte del contrato.

### 2.0-B — ABI de sistemas generalizada

**Objetivo:** expresar fronteras nativas con menos reglas ad hoc.

- versión explícita de calling conventions, layout y clasificación de agregados;
- enums, handles, `Guid`, arrays, unions y structs anidados en todas las posiciones ABI;
- callbacks, delegates, vtables y COM estático sin runtime COM;
- UTF-8/UTF-16 y buffers con ownership explícito;
- SIMD e intrínsecos por familias justificadas;
- preparación para un target adicional sólo cuando exista implementación y matriz completas.

**Gate:** comparación física contra clang-cl/MSVC y ejecución bidireccional. No se promete ABI de
clases C++.

### 2.0-C — Genéricos y bibliotecas como contrato binario

**Objetivo:** que la monomorfización AOT sea componible a escala de ecosistema.

- identidad canónica de templates y especializaciones;
- clausura pública/privada precisa;
- constraints e interfaces preservados entre módulos;
- política ODR/COMDAT por símbolo físico;
- metadata versionada con migración o rechazo accionable;
- composición estática y dinámica sin depender de materialización incidental del productor.

**Gate:** matriz productor/consumidor N−1/N, múltiples bibliotecas, estática/DLL y ejecución real.

### 2.0-D — Debug extremo como contrato del lenguaje

**Objetivo:** hacer diagnosticable el modelo nativo completo, no sólo instrucciones y nombres.

- PDB estándar para EXE y bibliotecas;
- líneas, columnas, scopes, parámetros, locales y capturas;
- valores en registros, stack, constantes, fragmentos u optimizados fuera;
- clases, interfaces, genéricos, closures, delegates y ownership;
- stack correcto a través de SEH, thunks, callbacks y dispatch virtual;
- identidad de fuente reproducible y remapeable;
- handoff equivalente para los linkers soportados;
- contrato consumible por Visual Studio, WinDbg y NativeSharp Debugger.

**Gate:** sesión real O0 y, cuando se autorice la certificación correspondiente, diagnóstico de
builds optimizados sin inventar valores.

### 2.0-E — Fronteras internas del compilador

**Objetivo:** separar semántica, runtime y backend para que el producto sea mantenible.

- decisiones semánticas completas antes de LLVM;
- frontera MIR estable para lowering y diagnósticos;
- subconjunto LLVM encapsulado y versionado;
- runtime nativo con ABI explícita y componentes reemplazables;
- menos conocimiento cruzado entre parser, importador, metadata y linker;
- verificadores locales en cada frontera.

Esto no obliga a crear otro backend ni a reescribir NSC en NativeSharp. La posibilidad futura de
self-hosting será una consecuencia de madurez, no un gate.

### 2.0-F — Toolchain y distribución

**Objetivo:** que el producto se use fuera de los árboles de desarrollo sin procedimientos
privados.

- builds reproducibles e identidad trazable;
- proyecto, paquetes y dependencias con formatos versionados;
- Debug/Release y targets declarados;
- actualización y rollback seguros;
- diagnósticos de instalación y compatibilidad;
- documentación versionada, migración 1.x→2.0 y notas de ruptura;
- matriz de compilador, runtime, NSMETA, ABI y linkers.

**Gate:** una máquina limpia construye, enlaza, depura y verifica un consumidor representativo.

### 2.0-G — Candidato y promoción

La promoción requiere:

1. congelación de gramática, especificación y ABI 2.0;
2. migración documentada desde la última 1.x soportada;
3. cero P0/P1 conocido y cero miscompile reproducible abierto;
4. corpus positivo y negativo;
5. bibliotecas precompiladas y PDB verificadas de extremo a extremo;
6. instalación limpia y reproducible;
7. periodo RC sin cambios semánticos silenciosos;
8. decisión explícita de release.

## 11. Orden de ejecución / Execution order

```text
development (1.0.1-dev)
  |
  +-- dev-000  punto inicial congelado
  |
  +-- dev-001..NNN
  |     fixes de consumidores + NSMETA/DLL + PDB + documentación
  |
  +-- 1.0.1-rc
  |     baseline explícito + instalación limpia + estabilización
  |
  +-- 1.0.1
  |     contrato compatible promovido
  |
  +-- 1.x
  |     correctness, toolchain, runtime y ecosistema compatibles
  |
  +-- 2.0-design
  |     libro mayor de rupturas + contratos + migración
  |
  +-- 2.0-dev
  |     A semántica -> B ABI -> C bibliotecas -> D debug
  |     -> E arquitectura -> F distribución
  |
  `-- 2.0-rc -> 2.0
        certificación y promoción explícitas
```

Los defectos de consumidor P0 pueden reordenar tareas dentro de una fase. No autorizan mezclar
repositorios, modificar el debugger, implementar el linker dentro de NSC ni ejecutar baselines no
solicitados.

## 12. Disciplina de entregas / Delivery discipline

Cada cambio de implementación incluye:

- repro o consumidor identificable;
- regla afectada;
- modificación limitada al repositorio y subsistema autorizados;
- prueba dirigida proporcional;
- Markdown bilingüe con archivos, funciones, implementación, razones y evidencia;
- commit local con título y descripción en español e inglés.

La política activa distingue cuatro niveles:

| Nivel | Uso | Ejecución predeterminada |
|---|---|---|
| Inspección | diagnóstico y lectura | sin escritura ni pruebas amplias |
| Iteración | corregir un defecto | O0 y prueba mínima dirigida |
| Integración | cerrar un lote | compilar el compilador completo si se solicita |
| Certificación | promover snapshot/release | baseline únicamente por orden explícita |

No se corre el corpus de bindings desde NSC salvo autorización. El responsable del consumidor
valida el build instalado y devuelve el siguiente defecto mínimo.

## 13. Matriz de propiedad / Ownership matrix

| Superficie | Dueño principal | Responsabilidad de NSC |
|---|---|---|
| Lenguaje y lowering | `A:\nsc-devel` | semántica, diagnósticos e IR válido |
| NSMETA y producto de biblioteca | `A:\nsc-devel` | metadata y objetos consumibles |
| PDB producido por NSC | `A:\nsc-devel` | CodeView, tipos, líneas y handoff correcto |
| Linker NSLink | repositorio NSLink | COFF/PE, COMDAT, PDB final y diagnósticos de enlace |
| Debugger | repositorio del debugger | consumo de PDB, control del proceso y UI; sólo lectura desde NSC |
| Bindings | repositorio de bindings | superficie y adaptación de APIs externas |
| Frameworks/juegos | su repositorio | port y diseño de aplicación, no semántica del compilador |

Un error puede cruzar fronteras, pero cada corrección se aplica en el dueño de la causa. Un
documento de handoff explica el contrato requerido sin modificar el repositorio ajeno.

## 14. Riesgos que gobiernan prioridades / Priority-driving risks

1. **Miscompile silencioso:** prioridad máxima, incluso sobre compatibilidad accidental.
2. **Metadata aceptada pero inutilizable:** debe rechazarse en el productor.
3. **Diferencia ABI entre rutas:** requiere repro físico y comparación externa.
4. **Símbolos ODR/COMDAT:** se decide por identidad física, no por spelling incidental.
5. **PDB que inventa valores:** es peor que marcar un valor no disponible.
6. **Workaround de consumidor convertido en diseño:** debe retirarse al corregir la causa.
7. **Baseline excesivo durante iteración:** consume recursos y retrasa el barrido de defectos.
8. **Ruptura 2.0 sin migración:** no se acepta como progreso arquitectónico.
9. **Feature por paridad:** infla lenguaje y runtime sin resolver un caso nativo.
10. **Snapshot mutable:** destruye la utilidad histórica de la serie `dev-NNN`.

## 15. Siguiente hito recomendado / Recommended next milestone

El trabajo posterior a `dev-000` debe acumularse en `development` hasta cerrar un lote real de
defectos reportados por consumidores y su documentación. Entonces se crea
`nativesharp-1.0.1-dev-001` sobre el commit resultante.

Antes de `dev-001` no hace falta inventar una feature nueva. El orden recomendado es:

1. terminar los defectos P0 concretos de NSMETA/DLL/ODR que aparezcan en consumidores;
2. conservar verde la prueba dirigida de cada corrección;
3. cerrar el handoff PDB que necesite el debugger sin modificarlo;
4. mantener documentación y versión instalable coherentes;
5. crear el snapshot sin baseline;
6. ejecutar compilación completa, instalación o baseline sólo cuando se solicite.

## 16. Definición de éxito / Definition of success

### Para 1.0.1

Un consumidor real puede compilar, enlazar, ejecutar y depurar usando el producto instalado y las
bibliotecas precompiladas, con diagnósticos reproducibles y sin workarounds que contradigan el
contrato.

### Para 1.x

El ecosistema crece sin romper ABI, metadata ni semántica pública; las correcciones llegan como
entregas pequeñas, trazables y verificables.

### Para 2.0

NativeSharp puede realizar un cambio mayor necesario de manera deliberada: contrato escrito,
impacto medido, migración disponible, toolchain completo y evidencia de extremo a extremo. La
versión 2.0 no se mide por cantidad de sintaxis, sino por coherencia de semántica, ABI, bibliotecas,
debug y distribución.

---

## English executive summary

NativeSharp 1.0.1-dev already has a substantial native language, Windows x64 ABI, precompiled
library, packaging, and PDB producer foundation. The immediate job is to close consumer-reported
P0 defects, finish the precompiled-library and debugger handoffs, keep current documentation
installable, and explicitly promote 1.0.1 only after requested certification.

The 1.x line is compatibility-preserving. NativeSharp 2.0 starts only when a necessary change
cannot be delivered honestly within that compatibility contract. Every breaking proposal needs a
consumer, an affected 1.x contract, rejected compatible alternatives, a migration path, component
versioning, and executable evidence. The 2.0 program consolidates semantics, system ABI,
monomorphized libraries, extreme native debugging, compiler boundaries, and distribution without
turning NativeSharp into C#, adding a managed runtime, or requiring self-hosting.
