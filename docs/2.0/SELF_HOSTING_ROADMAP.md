# NativeSharp 2.0 — roadmap autocontenido de self-hosting

**Estado del documento:** plan normativo de ingeniería  
**Edición objetivo:** NativeSharp 2.0  
**Plataforma inicial:** Windows x64  
**Compilador de partida:** NSC 2.0, Stage 70  
**Fecha de corte:** 2026-09-18  
**Revisión de partida:** `1601ee91005809995abeb17878b3fb89722b84c6`  
**Baseline de partida:** Gate 0 `LANDED`, corrida `20260918-001908-7afc3d28`

## 1. Propósito

Este documento define todo el trabajo necesario para que el compilador de NativeSharp pase de la
implementación actual en C++ (`nsc0`) a una implementación escrita en NativeSharp (`nsc-next`) y
termine siendo capaz de compilarse a sí misma de forma correcta, determinista y reproducible.

Es autocontenido: fija vocabulario, alcance, arquitectura, dependencias permitidas, fases, tareas,
gates, matrices de prueba, artefactos, política de equivalencia, migración y criterio de cierre. Los
documentos históricos bajo `mkdown/` sirven como evidencia, pero no son necesarios para ejecutar
este plan ni pueden contradecirlo.

El objetivo no es traducir mecánicamente cada archivo C++ ni eliminar LLVM. El objetivo es producir
un compilador NativeSharp mantenible que preserve el contrato observable de NSC 2.0 y cuya cadena de
bootstrap pueda demostrarse.

## 2. Resultado final requerido

Al terminar deben existir cuatro generaciones claramente identificadas:

| Nombre | Implementación | Compilado por | Función |
|---|---|---|---|
| `nsc0` | C++ actual | toolchain C++ certificado | semilla y oráculo temporal |
| `nsc1` | NativeSharp | `nsc0` | primera generación NativeSharp |
| `nsc2` | NativeSharp | `nsc1` | primera generación autoalojada |
| `nsc3` | NativeSharp | `nsc2` | prueba de punto fijo y reproducibilidad |

El proyecto se considera autoalojado únicamente cuando:

1. `nsc0` construye `nsc1` desde un árbol limpio.
2. `nsc1` construye `nsc2` sin usar el frontend, parser, semántica ni generadores de código de
   `nsc0` durante esa compilación.
3. `nsc2` construye `nsc3` con las mismas entradas bloqueadas.
4. `nsc2` y `nsc3` son equivalentes conforme a la política de la sección 15.
5. `nsc2` supera la matriz completa de NSC 2.0 con `lld-link` y NSLink.
6. La instalación, los metadatos, el runtime y las bibliotecas producidos se verifican igual que los
   del baseline oficial.
7. La construcción es repetible en una máquina limpia usando sólo dependencias documentadas.
8. Existe una ruta de recuperación probada mediante `nsc0` durante la ventana de transición.

## 3. Estado real de partida

### 3.1 Evidencia certificada

El corte inicial tiene estas propiedades verificadas:

- compilador instalado: `nsc 2.0.0+build.20260918T062024Z.376cf31bfd872e0d`;
- Stage 70;
- compilación Release limpia aprobada;
- CTest: 435/435 con `lld-link` y 435/435 con NSLink;
- fixtures: 148/148 por enlazador;
- bug triage: 18/18 por enlazador;
- total observado: 1,202 ejecuciones aprobadas y cero fallos;
- Gate 0: `certified: true`, `state: LANDED`, `problems: []`;
- el compilador actual está compuesto por 191 archivos `.cpp` distribuidos entre AST, Basic, CFG,
  Compilation, Diagnostics, Dominance, Driver, Frontend, HIR, Interop, Lexer, Linker, LLVM, MIR,
  Parser, Passes, Runtime, Sema, Source, SSA y Target;
- la biblioteca estándar de NativeSharp contiene 80 fuentes `.ns`.

El comando canónico que debe seguir protegiendo cada hito es:

```text
python nscbuilder.py --baseline --jobs N
```

`N` es una decisión de la máquina ejecutora. El plan no fija un número universal de workers.

### 3.2 Capacidades ya disponibles

No deben volver a planificarse como si faltaran:

- ownership, `owned`, `borrowed`, RAII y análisis de escape;
- punteros, function pointers, delegates, callbacks y ABI C/C++/Win64;
- genéricos monomorfizados, constraints, inferencia y referencias entre proyectos;
- excepciones, EH Win64, `try`/`catch`/`finally` y limpieza;
- arreglos fijos, multidimensionales, VLA, `stackalloc`, placement y destrucción explícita;
- `String`, `StringView`, `Span<T>`, `ReadOnlySpan<T>` y `Optional<T>`;
- `List<T>`, `Dictionary<K,V>`, `HashSet<T>`, hashing, comparadores y algoritmos básicos;
- `ByteBuffer`, `BinaryReader`, `BinaryWriter`, lectura y escritura binaria;
- `Path`, `File`, `Directory`, enumeración y operaciones de filesystem;
- consola con canales separados, formateo y parsing numérico;
- `Process.Run`, argumentos de línea de comandos y diagnóstico del sistema;
- atomics, mutex, monitor, semáforos, eventos, threads, jobs y thread pool;
- `nsproj`, referencias, bibliotecas estáticas/dinámicas, `nsmeta`, SDK declarativo y caché;
- backend LLVM, O0/O1/O2/O3, PDB/CodeView, LTO/ThinLTO y dos enlazadores;
- NSLink, `lld-link`, runtime ABI versionado y verificación física de artefactos.

Estas capacidades necesitan pruebas específicas de uso por el compilador autoalojado, pero no una
segunda implementación sin evidencia de defecto.

### 3.3 Elementos que todavía no existen

Éstos son los bloqueos actuales del self-hosting:

1. No existe un Bootstrap Profile 2.0 congelado y comprobable automáticamente.
2. No existe `nsc-next` escrito en NativeSharp.
3. No existe un paquete NativeSharp versionado para el subconjunto usado de la API C de LLVM.
4. `WindowsRuntimeBuilder` continúa embebido en la implementación C++.
5. No existe una frontera estable que permita consumir el runtime sin regenerarlo desde C++.
6. No existe un harness diferencial sistemático `nsc0` contra `nsc-next` para cada etapa.
7. No existe automatización `nsc0 → nsc1 → nsc2 → nsc3`.
8. No existe un contrato de equivalencia binaria/semántica para `nsc2` y `nsc3`.
9. No existe un gate separado de self-hosting ni un manifiesto de sus dependencias.
10. No existe todavía una política de corte, fallback y retiro gradual de `nsc0`.

## 4. Alcance

### 4.1 Incluido

- Compilador completo de NativeSharp 2.0 escrito en NativeSharp.
- Windows x64 y ABI MSVC/COFF/PE como primer host y target.
- Frontend, semántica, HIR, MIR, CFG, SSA, backend LLVM, driver, proyectos, diagnósticos y caché.
- Consumo de LLVM mediante su API C congelada.
- Producción de objetos, ejecutables, bibliotecas, PDB, `nsmeta` y manifiestos compatibles.
- Enlace con NSLink y `lld-link`.
- Construcción reproducible de cuatro generaciones.
- Matrices diferenciales y baseline completo.
- Instalación, rollback y transición desde el compilador C++.

### 4.2 Dependencias externas permitidas

Self-hosting no significa rehacer todo el toolchain. Se permiten, siempre que estén bloqueadas y
registradas:

- LLVM y su API C;
- `lld-link`;
- NSLink como producto hermano;
- Windows SDK y bibliotecas del sistema;
- CMake/Ninja/Python únicamente como orquestadores de transición;
- el compilador C++ anfitrión sólo para producir o recuperar `nsc0`;
- archivos de runtime y bindings precompilados cuya procedencia y hash estén en el manifiesto.

Ninguna dependencia permitida puede ocultar una segunda implementación del frontend o de la
semántica de NativeSharp.

### 4.3 Fuera de alcance

- Reemplazar LLVM por un backend propio.
- Reescribir NSLink en NativeSharp como requisito del primer bootstrap.
- Añadir características de NativeSharp 3.0.
- Cambiar el ABI público, `nsmeta`, runtime ABI o semántica del lenguaje para facilitar el port.
- Buscar paridad de rendimiento antes de paridad de corrección.
- Soportar Linux, macOS, ARM64 o cross-compilation en el primer gate.
- Eliminar inmediatamente el código C++ después del primer bootstrap.
- Convertir el compilador en multithread si la primera generación correcta necesita operar con un
  perfil single-thread.

## 5. Principios no negociables

1. **Una fuente de verdad.** La especificación de NativeSharp 2.0 y sus contratos versionados mandan;
   `nsc0` es un oráculo temporal, no una nueva especificación.
2. **Bootstrap sin circularidad.** `nsc-next` sólo puede usar características que el compilador que
   lo construye ya implementa y que pertenecen al perfil congelado.
3. **Cambios pequeños y verificables.** Cada tarea numerada debe poder aterrizar con pruebas verdes.
4. **Diferencial antes de sustitución.** Cada subsistema portado se compara contra `nsc0` antes de
   retirar el camino anterior.
5. **Determinismo desde el inicio.** Orden, rutas, timestamps, IDs y hashes no se corrigen al final.
6. **Sin doble semántica.** No se mantienen dos reglas activas para tipos, overloads, ownership o ABI.
7. **Fronteras explícitas.** Plataforma, LLVM, runtime, filesystem, procesos y linkers están detrás de
   interfaces pequeñas y versionadas.
8. **Errores reproducibles.** Diagnósticos, orden y códigos de salida forman parte del contrato.
9. **Baseline siempre recuperable.** Ningún hito destruye la capacidad de construir `nsc0`.
10. **Cero atajos instalados.** Las pruebas usan artefactos de la corrida, no binarios casuales del
    `PATH` ni contenido residual de `C:\native-sharp`.

## 6. Arquitectura objetivo

El árbol NativeSharp del compilador debe quedar dividido por contratos y no por la forma accidental
de las clases C++ actuales:

```text
selfhost/
  bootstrap/          perfil, lockfiles, manifiestos y orquestación
  nsc/
    Basic/            IDs, hashes, arenas, utilidades deterministas
    Source/           archivos, buffers, posiciones y line maps
    Diagnostics/      códigos, argumentos, render y bundles
    Lexer/            tokens, trivia, literales y Unicode/UTF-8
    Syntax/           nodos inmutables, factories y dumper canónico
    Parser/           declaraciones, tipos, expresiones y recuperación
    Symbols/          scopes, nombres, módulos e identidad nominal
    Types/            tipos, layouts, conversiones y substituciones
    Sema/             binding, overloads, flujo, ownership y constantes
    HIR/              representación semántica independiente del parser
    MIR/              operaciones explícitas y contrato de runtime
    CFG/              bloques, aristas, dominancia y análisis de datos
    SSA/              construcción, verificación y transformaciones
    CodeGen.LLVM/     lowering mediante el binding de LLVM C
    Target.Win64/     ABI, COFF, PE, calling conventions y CodeView
    Metadata/         lectura/escritura determinista de nsmeta
    Projects/         nsproj, referencias, SDK y grafo de compilación
    Driver/           CLI, pipeline, perfiles y códigos de salida
    Cache/            claves, invalidación y artefactos por contenido
  bindings/
    LLVM.C/           subconjunto exacto, versionado y probado
    Win32.Compiler/   frontera mínima de plataforma
  tests/
    differential/     nsc0 contra nsc-next
    bootstrap/        nsc0/nsc1/nsc2/nsc3
    fixtures/         corpus exclusivo del compilador autoalojado
```

La ruta exacta podrá ajustarse una sola vez en la Fase 0. Después de congelarla, mover módulos exige
una decisión registrada para no invalidar diagnósticos, PDB y manifiestos.

## 7. Artefactos normativos

El programa debe producir y conservar como mínimo:

| Artefacto | Contenido |
|---|---|
| `BootstrapProfile-2.0.md` | subconjunto de lenguaje y biblioteca autorizado |
| `bootstrap.lock` | versiones y hashes de nsc0, LLVM, linkers, SDK y runtime |
| `llvm-c-api.contract` | símbolos, tipos, calling conventions y versión aceptada |
| `runtime-bootstrap.contract` | ABI y hashes del runtime consumido por cada generación |
| `diagnostics.map` | correspondencia estable entre diagnósticos nsc0/nsc-next |
| `equivalence.contract` | reglas exactas de comparación nsc2/nsc3 |
| `bootstrap-manifest.json` | entradas y salidas de cada generación |
| `report.md` | resumen humano de la corrida |
| `report.json` | resultados estructurados y consumibles por CI |
| `gate.json` | estado, problemas, hashes, revisión y evidencia del gate |
| SBOM/manifiesto de instalación | inventario final de archivos instalables |

Los formatos deben tener versión y lector que rechace versiones futuras desconocidas.

## 8. Roadmap por fases

Cada identificador `NS2-SH-xxx` representa una unidad de trabajo que puede convertirse en un commit
independiente. Una fase no termina por porcentaje: termina cuando su gate es reproducible.

### Fase 0 — congelar el programa y proteger el punto de partida

**Objetivo:** evitar que el proyecto de self-hosting cambie de definición mientras se implementa.

- `NS2-SH-000` Registrar revisión, versión, hashes y reporte del baseline inicial.
- `NS2-SH-001` Crear el directorio canónico `selfhost/` sin mover código de producción.
- `NS2-SH-002` Fijar los nombres `nsc0`, `nsc1`, `nsc2` y `nsc3` en contratos y reportes.
- `NS2-SH-003` Escribir el inventario completo de dependencias externas permitidas.
- `NS2-SH-004` Definir la estructura y versión de `bootstrap.lock`.
- `NS2-SH-005` Crear un verificador de hashes y versiones del entorno bloqueado.
- `NS2-SH-006` Separar artefactos de self-hosting de los builds normales.
- `NS2-SH-007` Añadir limpieza dirigida que nunca toque fuentes ni instalaciones ajenas.
- `NS2-SH-008` Crear el formato de reportes y el directorio de evidencia inmutable.
- `NS2-SH-009` Documentar recuperación desde `nsc0` y restauración de la última generación válida.

**Gate SH00:** baseline normal verde; verificador del entorno verde; ningún archivo de usuario o
instalación no administrada entra en el conjunto de limpieza.

Expediente de cierre: `docs/2.0/self-hosting/tandas/TANDA-01-SH00-INFRAESTRUCTURA.md`.
Gate versionado: `selfhost/evidence/gates/SH00/gate.json`.

### Fase 1 — Bootstrap Profile 2.0

**Objetivo:** congelar exactamente el lenguaje con el que se escribirá `nsc-next`.

- `NS2-SH-010` Inventariar cada construcción sintáctica usada por el diseño del compilador.
- `NS2-SH-011` Congelar tipos primitivos, casts, promociones y overflow permitidos.
- `NS2-SH-012` Congelar clases, structs, interfaces, enums, aliases y tipos anidados permitidos.
- `NS2-SH-013` Congelar genéricos, constraints, inferencia y monomorfización permitidos.
- `NS2-SH-014` Congelar reglas de ownership, préstamos, movimiento, destrucción y escape.
- `NS2-SH-015` Congelar excepciones, cleanup y límites donde se prohíben excepciones.
- `NS2-SH-016` Congelar punteros, function pointers, delegados y operaciones unsafe.
- `NS2-SH-017` Congelar el subconjunto de stdlib y sus versiones de ABI.
- `NS2-SH-018` Crear casos positivos y negativos para cada entrada del perfil.
- `NS2-SH-019` Implementar un verificador que impida introducir fuera de perfil en `selfhost/nsc`.

**Gate SH01:** el perfil tiene cobertura ejecutable; una construcción fuera del perfil falla antes
de iniciar el bootstrap; ninguna característica depende de `nsc-next` para ser compilada.

Expediente de cierre: `docs/2.0/self-hosting/tandas/TANDA-02-SH01-PERFIL-BOOTSTRAP.md`.
Contrato normativo: `docs/2.0/self-hosting/BootstrapProfile-2.0.md` y
`selfhost/bootstrap/bootstrap-profile.json`.
Gate versionado: `selfhost/evidence/gates/SH01/gate.json`.

### Fase 2 — biblioteca anfitriona y determinismo básico

**Objetivo:** demostrar que la stdlib actual cubre cargas reales de un compilador.

- `NS2-SH-020` Crear pruebas de estrés de `String`, `StringView` y UTF-8 con fuentes hostiles.
- `NS2-SH-021` Certificar `Span`, `ReadOnlySpan` y escape bajo arenas y buffers de fuente.
- `NS2-SH-022` Certificar `Optional`, listas, diccionarios, sets y hashing determinista.
- `NS2-SH-023` Certificar sorting estable y algoritmos requeridos por tablas de símbolos.
- `NS2-SH-024` Certificar lectura/escritura binaria y entradas truncadas o corruptas.
- `NS2-SH-025` Certificar rutas largas, Unicode, separadores y enumeración ordenada.
- `NS2-SH-026` Certificar stdout, stderr, códigos de salida y ejecución de procesos.
- `NS2-SH-027` Añadir arenas/pools NativeSharp con ownership explícito y contadores de fuga.
- `NS2-SH-028` Definir contenedores o adaptadores que falten con pruebas, no por equivalencia C++.
- `NS2-SH-029` Medir asignaciones, pico de memoria y tiempo de las cargas representativas.

**Gate SH02:** un programa NativeSharp de prueba carga el corpus, crea tokens sintéticos, llena
tablas, emite diagnósticos y ejecuta un linker sin fugas ni orden no determinista.

Expediente de cierre: `docs/2.0/self-hosting/tandas/TANDA-03-SH02-BIBLIOTECA-ANFITRIONA.md`.
Carga certificada: `selfhost/tests/host/CompilerHostWorkload/` y
`selfhost/tests/host/corpus/`.
Gate versionado: `selfhost/evidence/gates/SH02/gate.json`.

### Fase 3 — fronteras LLVM, plataforma, runtime y enlace

**Objetivo:** eliminar dependencias C++ ocultas antes de portar el frontend.

- `NS2-SH-030` Extraer el conjunto exacto de símbolos LLVM C usado por NSC.
- `NS2-SH-031` Congelar tipos opacos, enums, firmas y calling conventions del binding.
- `NS2-SH-032` Crear `NativeSharp.LLVM.C` con carga y validación de versión.
- `NS2-SH-033` Añadir smoke tests por familia LLVM: contexto, módulo, tipos, IRBuilder y verificador.
- `NS2-SH-034` Cubrir target machine, data layout, optimización, object emission y debug info.
- `NS2-SH-035` Rechazar en runtime una LLVM incompatible con diagnóstico estable.
- `NS2-SH-036` Externalizar el runtime generado a artefactos versionados y verificables.
- `NS2-SH-037` Definir `runtime-bootstrap.contract` y prueba de compatibilidad ABI.
- `NS2-SH-038` Crear la frontera NativeSharp mínima de Win32 requerida por el compilador.
- `NS2-SH-039` Encapsular filesystem, procesos, memoria mapeada y reloj detrás de servicios.
- `NS2-SH-040` Crear adaptadores independientes para NSLink y `lld-link`.
- `NS2-SH-041` Verificar quoting, rutas con espacios, response files y códigos de salida.
- `NS2-SH-042` Producir un objeto y un ejecutable mínimo desde NativeSharp vía LLVM C.
- `NS2-SH-043` Verificar ese ejecutable con ambos linkers y con el verificador PE existente.
- `NS2-SH-044` Congelar la frontera; cualquier símbolo nuevo exige contrato y prueba.

**Gate SH03:** una aplicación NativeSharp usa el binding para emitir un `.obj`, lo enlaza con ambos
linkers y ejecuta con el resultado esperado sin llamar a código del frontend C++.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-04-SH03-FRONTERAS-LLVM-RUNTIME-PLATAFORMA.md`.
Contratos congelados: `selfhost/contracts/llvm-c-api.contract.json` y
`selfhost/contracts/runtime-bootstrap.contract.json`.
Gate versionado: `selfhost/evidence/gates/SH03/gate.json`.

### Fase 4 — esqueleto de `nsc-next` y capa Basic

**Objetivo:** obtener un ejecutable NativeSharp instalable que todavía no compile, pero cuyo
comportamiento operativo ya sea correcto.

- `NS2-SH-045` Crear proyectos, módulos y entry point de `nsc-next`.
- `NS2-SH-046` Implementar identidad de versión, `--version`, `--help` y códigos de salida.
- `NS2-SH-047` Portar IDs tipados, hashing, resultados y utilidades deterministas.
- `NS2-SH-048` Implementar arenas y handles sin punteros colgantes.
- `NS2-SH-049` Implementar source buffers, file IDs, posiciones y rangos.
- `NS2-SH-050` Implementar line maps y normalización de finales de línea.
- `NS2-SH-051` Implementar catálogo estructurado de diagnósticos.
- `NS2-SH-052` Implementar render humano y salida estructurada.
- `NS2-SH-053` Normalizar rutas de diagnóstico sin depender del directorio de ejecución.
- `NS2-SH-054` Crear pruebas diferenciales de Basic/Source/Diagnostics contra `nsc0`.

**Gate SH04:** `nsc-next` arranca, reporta identidad, lee fuentes y reproduce los diagnósticos
básicos y códigos de salida acordados.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-05-SH04-ESQUELETO-BASIC-SOURCE-DIAGNOSTICS.md`.
Contrato de diagnósticos: `selfhost/contracts/diagnostics.map.json`.
Gate versionado: `selfhost/evidence/gates/SH04/gate.json`.

### Fase 5 — lexer, sintaxis y parser

**Objetivo:** aceptar todo NativeSharp 2.0 y producir árboles canónicos sin semántica.

- `NS2-SH-055` Portar clases de caracteres, UTF-8 y recorrido del buffer.
- `NS2-SH-056` Portar tokens, keywords, trivia y ubicaciones.
- `NS2-SH-057` Portar literales numéricos, caracteres y strings con overflow correcto.
- `NS2-SH-058` Portar lexer de ensamblador integrado y ubicaciones internas.
- `NS2-SH-059` Definir nodos de sintaxis inmutables y arenas.
- `NS2-SH-060` Portar parser de declaraciones, modifiers y tipos.
- `NS2-SH-061` Portar parser de statements y expresiones.
- `NS2-SH-062` Portar genéricos, constraints, atributos y ensamblador integrado.
- `NS2-SH-063` Portar recuperación de errores y límites de cascada.
- `NS2-SH-064` Crear dumps canónicos de tokens y árboles.
- `NS2-SH-065` Ejecutar diferencial sobre todas las fuentes válidas del repositorio.
- `NS2-SH-066` Ejecutar diferencial sobre corpus inválido, truncado, fuzz y regresiones.

**Gate SH05:** tokens y árboles coinciden con el contrato canónico; las diferencias aceptadas están
versionadas y no reducen diagnósticos ni recuperación.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-06-SH05-LEXER-SINTAXIS-PARSER.md`.
Contrato observable: `selfhost/contracts/frontend.contract.json`.
Gate versionado: `selfhost/evidence/gates/SH05/gate.json`.

### Fase 6 — símbolos, tipos, binding y semántica

**Objetivo:** producir un programa semánticamente ligado equivalente al compilador actual.

- `NS2-SH-067` Portar namespaces, scopes, imports y resolución de nombres.
- `NS2-SH-068` Portar identidad nominal, tipos calificados y tipos anidados.
- `NS2-SH-069` Portar tipos primitivos, punteros, referencias, arreglos y function pointers.
- `NS2-SH-070` Portar clases, structs, interfaces, enums, unions y bitfields.
- `NS2-SH-071` Portar members, accesibilidad, override, virtual y dispatch de interfaces.
- `NS2-SH-072` Portar overload resolution y ranking de conversiones.
- `NS2-SH-073` Portar inferencia y substitución genérica.
- `NS2-SH-074` Portar constraints y demanda de especializaciones.
- `NS2-SH-075` Portar constantes, promociones, casts y folding obligatorio.
- `NS2-SH-076` Portar definite assignment y análisis de flujo.
- `NS2-SH-077` Portar ownership, préstamos, movimientos y análisis de escape.
- `NS2-SH-078` Portar lambdas, closures, delegates y events.
- `NS2-SH-079` Portar excepciones, filtros, cleanup y flujo no local.
- `NS2-SH-080` Portar layout, alineación, calling conventions y contratos ABI.
- `NS2-SH-081` Portar importación/exportación de símbolos `nsmeta`.
- `NS2-SH-082` Crear dump semántico canónico y comparador diferencial.
- `NS2-SH-083` Cerrar toda divergencia con fixture mínima antes de seguir.

**Gate SH06:** todos los casos de lenguaje alcanzan el mismo resultado semántico observable; las
tablas públicas, layouts y especializaciones son estables y deterministas.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-07-SH06-SIMBOLOS-TIPOS-SEMANTICA.md`.
Contrato observable: `selfhost/contracts/semantic.contract.json`.
Gate versionado al cierre: `selfhost/evidence/gates/SH06/gate.json`.

### Fase 7 — HIR, MIR, CFG, dominancia y SSA

**Objetivo:** independizar el backend del frontend y verificar cada frontera.

- `NS2-SH-084` Definir serialización/dump canónico versionado para HIR.
- `NS2-SH-085` Portar lowering semántica → HIR.
- `NS2-SH-086` Portar y activar el verificador HIR.
- `NS2-SH-087` Definir contrato y dump canónico para MIR.
- `NS2-SH-088` Portar lowering HIR → MIR.
- `NS2-SH-089` Portar operaciones núcleo, runtime y memoria a MIR explícito.
- `NS2-SH-090` Portar y activar el verificador MIR.
- `NS2-SH-091` Portar construcción y verificación de CFG.
- `NS2-SH-092` Portar recorridos, dominancia y dominance frontiers.
- `NS2-SH-093` Portar construcción de SSA y promoción de variables.
- `NS2-SH-094` Portar verificadores SSA y de ownership/cleanup.
- `NS2-SH-095` Portar passes requeridos para corrección antes que optimización.
- `NS2-SH-096` Portar passes O0/O1/O2/O3 en orden de dependencia.
- `NS2-SH-097` Comparar HIR/MIR/CFG/SSA por función contra `nsc0`.
- `NS2-SH-098` Añadir reducción automática del primer caso divergente.

**Gate SH07:** cada frontera acepta únicamente IR válido; el corpus completo llega a SSA y toda
divergencia se explica mediante contrato o se corrige.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-08-SH07-HIR-MIR-CFG-DOMINANCIA-SSA.md`.
Contrato observable: `selfhost/contracts/ir.contract.json`.
Gate versionado al cierre: `selfhost/evidence/gates/SH07/gate.json`.

### Fase 8 — backend LLVM, objetos y depuración

**Objetivo:** producir artefactos ejecutables compatibles con NSC 2.0.

- `NS2-SH-099` Portar mapeo de tipos y data layout a LLVM.
- `NS2-SH-100` Portar globals, funciones, bloques y control de flujo.
- `NS2-SH-101` Portar aritmética, casts, punteros, agregados y memoria.
- `NS2-SH-102` Portar llamadas, ABI Win64, varargs, vectorcall y callbacks.
- `NS2-SH-103` Portar objetos, dispatch, interfaces, delegates y closures.
- `NS2-SH-104` Portar EH Win64, cleanup, personality y unwind.
- `NS2-SH-105` Portar atomics, volatile, SIMD, intrinsics y ensamblador integrado.
- `NS2-SH-106` Portar generics materializados, COMDAT y deduplicación.
- `NS2-SH-107` Portar debug info, CodeView/PDB e inline sites.
- `NS2-SH-108` Portar pipelines O0/O1/O2/O3 y opciones LTO/ThinLTO.
- `NS2-SH-109` Emitir IR, bitcode, objetos y assembly con nombres deterministas.
- `NS2-SH-110` Comparar símbolos, relocations, secciones, unwind y exports.
- `NS2-SH-111` Ejecutar la matriz ABI contra hosts C, C++, DLL y COM.
- `NS2-SH-112` Certificar objetos con `lld-link` y NSLink.

**Gate SH08:** el mismo programa produce comportamiento, ABI, metadata y depuración compatibles en
todos los perfiles; no se exige todavía hash idéntico al objeto de `nsc0`.

Expediente de cierre:
`docs/2.0/self-hosting/tandas/TANDA-09-SH08-BACKEND-LLVM-OBJETOS-DEPURACION.md`.
Contrato observable: `selfhost/contracts/llvm-backend.contract.json`.
Gate versionado al cierre: `selfhost/evidence/gates/SH08/gate.json`.

### Fase 9 — proyectos, driver, metadata, caché y toolchain

**Objetivo:** convertir el compilador por archivo en un reemplazo operativo de `nsc`.

- `NS2-SH-113` Portar CLI, response files, perfiles y precedencia de opciones.
- `NS2-SH-114` Portar carga y validación de `.nsproj`.
- `NS2-SH-115` Portar globs, exclusiones y orden canónico de fuentes.
- `NS2-SH-116` Portar referencias, grafo, ciclos y compilación transitive.
- `NS2-SH-117` Portar selección SDK, arquitectura y versión.
- `NS2-SH-118` Portar contratos de target, runtime, linker y lenguaje.
- `NS2-SH-119` Portar lector/escritor `nsmeta` con compatibilidad N-1/N.
- `NS2-SH-120` Portar bibliotecas estáticas, DLL, facade projects y consumidores.
- `NS2-SH-121` Portar caché por contenido e invalidación incremental.
- `NS2-SH-122` Portar scheduling determinista; paralelismo queda opcional para `nsc1`.
- `NS2-SH-123` Portar perfiles y bundle diagnóstico reproducible.
- `NS2-SH-124` Portar manifest, SBOM, verificación de instalación y hashes.
- `NS2-SH-125` Portar selección y ejecución de ambos linkers.
- `NS2-SH-126` Añadir compatibilidad de CLI automatizada contra `nsc0`.

**Gate SH09:** `nsc-next` compila proyectos reales, referencias, EXE, LIB y DLL mediante la misma
superficie pública y produce instalaciones verificables.

Expediente de cierre: `docs/2.0/self-hosting/tandas/TANDA-10-SH09-PROYECTOS-DRIVER-METADATA-CACHE.md`.
Contrato: `selfhost/contracts/project-driver.contract.json`.
Gate: `selfhost/evidence/gates/SH09/gate.json`.

### Fase 10 — primera generación `nsc1`

**Objetivo:** construir por primera vez el compilador NativeSharp con `nsc0`.

- `NS2-SH-127` Congelar las fuentes exactas que componen `nsc1`.
- `NS2-SH-128` Construir dependencias NativeSharp desde un árbol vacío.
- `NS2-SH-129` Construir `nsc1` con `nsc0` y registrar todas las entradas.
- `NS2-SH-130` Verificar imports, exports, runtime ABI y dependencias físicas.
- `NS2-SH-131` Ejecutar smoke suite de CLI, lexer, parser, semántica y codegen.
- `NS2-SH-132` Compilar las bibliotecas del perfil con `nsc1`.
- `NS2-SH-133` Compilar un corpus escalonado con `nsc1`.
- `NS2-SH-134` Ejecutar tests diferenciales de resultados y diagnósticos.
- `NS2-SH-135` Ejecutar fixtures O0/O1/O2/O3 con ambos linkers.
- `NS2-SH-136` Medir tiempo, memoria, tamaño y estabilidad sin imponer todavía paridad.
- `NS2-SH-137` Generar manifiesto y reporte firmados por hashes.
- `NS2-SH-138` Probar reconstrucción de `nsc1` en un directorio limpio distinto.

**Gate SH10:** `nsc1` compila el corpus acordado y todos los subsistemas necesarios para construir
la siguiente generación; `nsc0` sigue verde e instalable.

Expediente de cierre: `docs/2.0/self-hosting/tandas/TANDA-11-SH10-PRIMERA-GENERACION-NSC1.md`.
Contrato: `selfhost/contracts/first-generation.contract.json`.
Gate: `selfhost/evidence/gates/SH10/gate.json`.

### Fase 11 — `nsc2`, primer compilador autoalojado

**Objetivo:** eliminar a `nsc0` del camino de compilación del compilador NativeSharp.

- `NS2-SH-139` Construir `nsc2` usando exclusivamente `nsc1` como compilador NativeSharp.
- `NS2-SH-140` Verificar que ningún proceso invoque accidentalmente `nsc0`.
- `NS2-SH-141` Registrar process tree, comandos, variables relevantes y hashes.
- `NS2-SH-142` Ejecutar la suite propia del compilador con `nsc2`.
- `NS2-SH-143` Ejecutar bibliotecas, ejemplos y corpus de producto con `nsc2`.
- `NS2-SH-144` Ejecutar CTest, fixtures y bug triage con ambos linkers.
- `NS2-SH-145` Comparar diagnósticos, `nsmeta`, ABI y artefactos contra `nsc1`.
- `NS2-SH-146` Reparar toda dependencia de rutas o timestamps no declarada.
- `NS2-SH-147` Construir en una segunda ruta absoluta y comparar resultados.
- `NS2-SH-148` Construir con locale y timezone distintos donde el contrato lo permita.
- `NS2-SH-149` Registrar Gate SH11 con problemas vacíos.

**Gate SH11:** `nsc2` existe, compila sin ejecutar `nsc0` y supera la matriz funcional completa de
self-hosting.

Expediente de implementación:
`docs/2.0/self-hosting/tandas/TANDA-12-SH11-NSC2-TRANSICION-AUTOALOJADA.md`.
El expediente distingue obligatoriamente entre generación derivada de fuentes y reproducción de
imagen atestada; sólo la primera permite la promoción final de SH14.

### Fase 12 — `nsc3` y punto fijo reproducible

**Objetivo:** demostrar que el compilador autoalojado ya no cambia por generación.

- `NS2-SH-150` Construir `nsc3` exclusivamente con `nsc2`.
- `NS2-SH-151` Normalizar únicamente los campos autorizados por `equivalence.contract`.
- `NS2-SH-152` Comparar fuentes, comandos, IR, objetos, PDB, `nsmeta` y manifiestos.
- `NS2-SH-153` Exigir identidad byte a byte donde el formato ya es determinista.
- `NS2-SH-154` Exigir equivalencia estructural explícita donde el formato tenga variación permitida.
- `NS2-SH-155` Convertir cada diferencia no autorizada en una regresión mínima.
- `NS2-SH-156` Repetir `nsc2 → nsc3` tres veces en árboles limpios.
- `NS2-SH-157` Repetir en al menos una segunda máquina Windows x64 compatible.
- `NS2-SH-158` Verificar que `nsc3` también supera la matriz completa.
- `NS2-SH-159` Congelar hashes y registrar Gate SH12.

**Gate SH12:** `nsc2` y `nsc3` alcanzan el punto fijo definido y lo repiten en dos máquinas sin
diferencias no explicadas.

Expediente de implementación local:
`docs/2.0/self-hosting/tandas/TANDA-13-SH12-NSC3-PUNTO-FIJO.md`. Un resultado de una sola máquina
mantiene SH12 `OPEN`; la segunda atestación no puede sustituirse por otra ruta o linker local.

### Fase 13 — paridad completa, robustez y rendimiento

**Objetivo:** convertir el éxito de bootstrap en un producto sustituible.

- `NS2-SH-160` Ejecutar todo CTest con `nsc2` y `nsc3`, sin pruebas omitidas.
- `NS2-SH-161` Ejecutar fixtures y bug triage en O0/O1/O2/O3 con ambos linkers.
- `NS2-SH-162` Ejecutar matrices DLL, C/C++, COM, PDB, LTO y target consumidor.
- `NS2-SH-163` Ejecutar fuzzing de lexer/parser, metadata, COFF y entradas hostiles.
- `NS2-SH-164` Ejecutar sanitizers disponibles sobre bindings y runtime.
- `NS2-SH-165` Comparar latencia, memoria y tamaño contra `nsc0` por fase.
- `NS2-SH-166` Fijar presupuestos de regresión separados para debug y release.
- `NS2-SH-167` Optimizar sólo perfiles medidos y conservar resultados deterministas.
- `NS2-SH-168` Ejecutar builds prolongados, cancelación, fallos de disco y procesos interrumpidos.
- `NS2-SH-169` Auditar archivos temporales, limpieza y recuperación posterior a fallo.
- `NS2-SH-170` Certificar instalación lado a lado de `nsc0` y `nsc2`.
- `NS2-SH-171` Cerrar documentación, troubleshooting y guía de contribución.

**Gate SH13:** cero regresiones funcionales; rendimiento dentro del presupuesto aprobado; artefactos
y diagnósticos reproducibles; instalación y rollback probados.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-14-SH13-ROBUSTEZ-RENDIMIENTO.md`.

### Fase 14 — promoción y retiro gradual de `nsc0`

**Objetivo:** cambiar el compilador oficial sin perder una ruta segura de recuperación.

- `NS2-SH-172` Publicar una RC donde `nsc2` sea predeterminado y `nsc0` seleccionable.
- `NS2-SH-173` Hacer que el empaquetado oficial se construya con la cadena autoalojada.
- `NS2-SH-174` Ejecutar baseline final posterior al freeze de RC.
- `NS2-SH-175` Verificar compatibilidad N-1/N y proyectos consumidores.
- `NS2-SH-176` Publicar manifiestos, hashes, contratos y reporte de bootstrap.
- `NS2-SH-177` Mantener `nsc0` congelado durante al menos una ventana de release.
- `NS2-SH-178` Prohibir nuevas features en `nsc0`; sólo correcciones de recuperación.
- `NS2-SH-179` Retirar `nsc0` del build diario tras cumplir la ventana y los gates.
- `NS2-SH-180` Conservar receta, toolchain y fuentes suficientes para reconstruir la semilla.
- `NS2-SH-181` Declarar NativeSharp 2.0 self-hosted mediante acta de promoción.

**Gate SH14:** el release oficial usa el compilador NativeSharp; la semilla C++ está congelada,
reproducible y fuera del camino normal, pero todavía recuperable.

Expediente de decisión: `docs/2.0/self-hosting/tandas/TANDA-15-SH14-PROMOCION-CONDICIONADA.md`.
El verificador también certifica una decisión `withhold`; nunca transforma gates abiertos en una
promoción por presión de calendario.

El cierre de ejecución de las tandas 7–16 vive en
`docs/2.0/self-hosting/tandas/TANDA-16-CIERRE-AUDITADO.md`. Cerrar el expediente no cambia el estado
de SH12–SH14 ni equivale a retirar `nsc0`.

### Reapertura técnica — retiro del image replay

La tanda 17 elimina de la matriz activa el atajo `source-attested-image-replay` usado para estudiar
la orquestación de generaciones. A partir de SH15 ninguna solicitud `nsc1 -> nsc2` puede copiar la
imagen productora: mientras el backend del compilador NativeSharp no produzca el compilador desde
sus fuentes, la operación falla de forma cerrada con código 78, conserva una traza auditable y no
crea ejecutable. Esto reduce capacidad aparente, pero restablece la semántica estricta del roadmap.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-17-SH15-RETIRO-IMAGE-REPLAY.md`.

La tanda 18 abre SH16 con el primer codegen inequívocamente derivado del AST: expresiones enteras
de retorno en `Main` se bajan a LLVM-C, producen objetos distintos para fuentes distintas y se
ejecutan con `lld-link` y NSLink en O0 y O3. Es una rebanada real pero limitada; todavía no basta
para compilar las 21 fuentes del compilador.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-18-SH16-BACKEND-DERIVADO-DE-FUENTE.md`.

La tanda 19 abre SH17 y extiende esa rebanada a funciones `int` libres: declaraciones previas de
prototipos LLVM, hasta cuatro parámetros enteros, resolución semántica de identificadores y
llamadas directas. El fixture `Add(19, 23)` debe conservar el resultado 42 en O0–O3 y al enlazar
con `lld-link` o NSLink. SH17 sigue sin cubrir locales, control de flujo, memoria, métodos ni las
fuentes completas de `nsc-next`; por tanto no reabre todavía la generación nsc1 → nsc2.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-19-SH17-FUNCIONES-PARAMETROS-LLAMADAS.md`.

La tanda 20 abre SH18 con almacenamiento local `int`, asignación simple, comparaciones con signo,
`if` y `while`. Los símbolos semánticos identifican slots LLVM y cada ruta debe terminar con CFG
válido. El acumulador 1..6 certifica resultado 42 en O0–O3 y ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-20-SH18-LOCALES-CONTROL-DE-FLUJO.md`.

La tanda 21 abre SH19 y completa la familia elemental de bucles con `for`, `do`/`while`, `break` y
`continue`. Una pila de destinos LLVM preserva el bucle activo, incluidos bucles anidados, y separa
el destino de continuación del destino de salida. El fixture combina las cuatro construcciones y
debe devolver 42 en O0–O3 con ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-21-SH19-CONTROL-DE-BUCLES.md`.

La tanda 22 abre SH20 con escalares `bool` junto a `int` y funciones `void`: firmas LLVM tipadas,
parámetros/locales booleanos, literales, negación, comparaciones que producen valor y retornos sin
valor. Las llamadas consultan el tipo real de la función LLVM. El fixture debe devolver 42 en los
cuatro perfiles y ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-22-SH20-BOOL-VOID.md`.

La tanda 23 abre SH21 y completa la rebanada de expresiones escalares: resto, operaciones bit a bit,
desplazamientos, asignaciones compuestas, incremento/decremento prefijo y postfijo, cortocircuito
real de `&&`/`||` y operador condicional. Los operadores de control crean bloques y nodos `phi`, de
modo que el operando omitido no produce efectos laterales. El fixture debe devolver 42 en O0–O3
con ambos linkers y fallaría si cualquiera de los lados cortocircuitados se evaluara.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-23-SH21-EXPRESIONES-ESCALARES.md`.

La tanda 24 abre SH22 y añade todos los anchos enteros del perfil: `byte`, `char`, `short`,
`ushort`, `uint`, `long`, `ulong`, `nint` y `nuint`. Las conversiones semánticas se materializan con
truncado o extensión con signo correcta; división, resto, comparación y shift derecho distinguen
signed/unsigned. El gate cubre conversiones en locales, operandos, argumentos, retornos y resultados
de asignación compuesta.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-24-SH22-ANCHOS-ENTEROS.md`.

La tanda 25 abre SH23 y añade punteros opacos al lowering fuente: firmas y locales de puntero,
`null`, toma de dirección, desreferencia, indexación y `stackalloc`. `EmitLValue` centraliza la
dirección y el tipo de almacenamiento para que cargas y asignaciones compartan semántica. El gate
ejecuta el fixture en O0–O3 con LLD y NSLink.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-25-SH23-PUNTEROS-MEMORIA-LOCAL.md`.

La tanda 26 abre SH24 y completa los escalares primitivos con `float` y `double`: constantes,
aritmética, comparación IEEE, conversiones con enteros, casts e incremento/decremento. Los
parámetros se materializan en slots de entrada para conservar su semántica mutable. El objeto COFF
publica `_fltused`, requerido por el ABI MSVC, y se ejecuta en O0–O3 con LLD y NSLink.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-26-SH24-ESCALARES-FLOTANTES.md`.

### 8.27 Tanda 27 — SH25

La tanda 27 construye el grafo fuente determinista completo: 92 unidades de cinco proyectos en
postorden, con rutas relativas al proyecto propietario y las 22 unidades raíz verificadas por
lexer/parser en O0–O3. Los errores históricos de bibliotecas quedaron localizados en el parser y no
se confundieron con semántica multimódulo.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-27-SH25-GRAFO-FUENTE.md`.

### 8.28 Tanda 28 — SH26

La tanda 28 crea el conjunto semántico multimódulo de las 23 unidades raíz. Indexa 1113 exports,
detecta identidad duplicada y registra 2845 resoluciones hacia símbolos de otra unidad. Añade
consumo real de registros públicos `NSMETA/2` sin reparsear la biblioteca: la fixture positiva
importa y resuelve un callable exclusivamente desde metadata. Fixtures positiva y negativa fijan
metadata externa, overloads válidos y firmas duplicadas; las superficies son idénticas en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-28-SH26-SEMANTICA-MULTIMODULO.md`.

### 8.29 Tanda 29 — SH27

La tanda 29 reemplaza la emisión del primer archivo por un módulo LLVM del programa raíz completo.
Elimina el límite fijo de cuatro argumentos y deriva el nombre COFF de la identidad semántica
cualificada más la firma nominal. Un proyecto de tres unidades con una llamada de seis argumentos
ejecuta con código 42 en la matriz O0–O3 × LLD/NSLink, sin símbolos históricos especiales.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-29-SH27-BACKEND-LLVM-GENERAL.md`.

### 8.30 Tanda 30 — SH28

La tanda 30 introduce `ValidatedIrProgram` como única admisión al backend. HIR, MIR, CFG,
dominancia y SSA se construyen para funciones anidadas en namespaces, se validan como una unidad y
producen superficies idénticas entre O0–O3. La entrada pública `SyntaxTree + SemanticModel` fue
retirada y el gate SH27 dependiente conserva ejecución real con ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-30-SH28-IR-PROGRAMA-COMPLETO.md`.

### 8.31 Tanda 31 — SH29

La tanda 31 publica layout determinista de enums, structs, clases y campos, y baja globals escalares
mutables/constantes con inicialización estática. El gate detectó y corrigió relocations absolutas
`ADDR32`: la emisión PIC usa `REL32`, enlaza con LLD y NSLink y devuelve 42 en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-31-SH29-DATOS-LAYOUT.md`.

### 8.32 Tanda 32 — SH30

La tanda 32 completa la identidad semántica del modelo de objetos: tipo propietario, `this`,
`base`, campos implícitos, métodos, constructores, propiedades, indexers y operadores. El backend
escalar excluye explícitamente métodos de instancia sin receptor y ejecuta el método estático de la
fixture con LLD/NSLink en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-32-SH30-MODELO-OBJETOS.md`.

### 8.33 Tanda 33 — SH31

La tanda 33 emite literales globales UTF-8 terminados en cero, baja `char` y escapes, materializa la
conversión a `char*`, pliega concatenación literal e indexa bytes. El payload `México` y la
ejecución 42 se verifican en O0–O3 con ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-33-SH31-TEXTO-NATIVO.md`.

### 8.34 Tanda 34 — SH32

La tanda 34 baja arrays nativos de heap mediante `GetProcessHeap`/`HeapAlloc`, libera `delete[]`
mediante `HeapFree`, conserva `stackalloc` y usa GEP tipado para la indexación. La fixture mezcla
heap y pila, inspecciona las imports del objeto y devuelve 42 con LLD y NSLink en O0–O3.

Esta tanda cierra el subconjunto de memoria requerido por el compilador bootstrap. Los objetos de
array administrado con cabecera, comprobación dinámica de límites y spans de biblioteca se validan
en el cierre de biblioteca/runtime de SH48; no se atribuyen falsamente a este backend mínimo.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-34-SH32-ARRAYS-MEMORIA.md`.

### 8.35 Tanda 35 — SH33

La tanda 35 publica firmas `ref`/`out`/`in` como punteros ABI, declara imports `extern C` con nombre
sin decorar y baja funciones C variádicas con prefijo fijo. Las convenciones se aplican tanto a la
declaración como al call site; la fixture combina direcciones, varargs e import Win32 y devuelve 42
con LLD y NSLink en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-35-SH33-FIRMAS-INTEROP.md`.

### 8.36 Tanda 36 — SH34

La tanda 36 conserva los argumentos de tipo escritos en cada call site y construye un registro AOT
canónico: identidad por nombre, tipos y aridad, deduplicación y orden ordinal independientes del
recorrido. Constraints y parámetros permanecen en AST/símbolos y el registro es idéntico en O0–O3.
La fixture ejecutable contiene dos usos de una misma instancia y certifica una sola materialización.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-36-SH34-GENERICOS-AOT.md`.

### 8.37 Tanda 37 — SH35

La tanda 37 preserva ownership en tipos compuestos, baja `move`, registra liberación explícita y
materializa `cleanup.if-live` en orden inverso antes de cada retorno. El verificador de flujo exige
que una función con owners no tenga salidas sin cleanup; la fixture transfiere un bloque heap,
lo usa, libera y devuelve 42 en O0–O3 con ambos linkers.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-37-SH35-OWNERSHIP-CLEANUP.md`.

### 8.38 Tanda 38 — SH36

La tanda 38 construye un grafo canónico de callables que distingue delegates, lambdas, llamadas,
virtuales y overrides. `--dump-project-callables` ofrece una identidad determinista consumible por
la materialización AOT, metadata y depurador; la fixture certifica la superficie y ejecuta una
llamada nativa hasta 42 con LLD/NSLink en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-38-SH36-CALLABLES.md`.

### 8.39 Tanda 39 — SH37

La tanda 39 materializa `default(T)`, `sizeof` y `alignof` como constantes tipadas y lleva
`static_assert` por el pipeline validado. El gate inspecciona AST y ejecuta el cálculo de layout a
42 con LLD y NSLink en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-39-SH37-PERFIL-BOOTSTRAP.md`.

### 8.40 Tanda 40 — SH38

La tanda 40 publica el grafo EH del proyecto y baja la ruta normal de `try/finally` sin perder el
estado local. El gate exige regiones canónicas, metadata Win64 `.pdata/.xdata` en cada objeto y
ejecución 42 con LLD/NSLink en O0–O3. `catch`, `throw` y `using` permanecen representados en el
grafo para que runtime y debugger compartan la misma numeración de regiones.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-40-SH38-EH-WIN64.md`.

### 8.41 Tanda 41 — SH39

La tanda 41 fija el plan de producto del driver para EXE, DLL, LIB y fachada: objeto, producto,
metadata, response file, perfil y linker. El gate verifica los cuatro planes, ejecuta response
files y enlaza el EXE con LLD/NSLink en O0–O3.

Expediente: `docs/2.0/self-hosting/tandas/TANDA-41-SH39-DRIVER-PRODUCTOS.md`.

### 8.42 Plan cerrado restante — Tandas 42–56

La auditoría posterior a SH27 confirma que `nsc-next` ya posee grafo fuente, identidad semántica
entre unidades y emisión LLVM ejecutable del programa raíz completo. Eso no equivale aún a
self-hosting: el backend todavía no consume exclusivamente IR validado y continúa limitado a una
rebanada escalar; la generación conserva
contratos históricos pendientes y faltan superficies obligatorias de NativeSharp 2.0. Las tandas
restantes cierran esas brechas sin introducir una segunda lista futura.

| Tanda | Gate | Resultado obligatorio de cierre |
|---:|---:|---|
| 42 | SH40 | Cierre de dependencias bootstrap: Core, Collections, IO y LLVM.C se reconstruyen desde fuente de forma reproducible, sin objetos ocultos de `nsc0`. |
| 43 | SH41 | Primera autocompilación real `nsc1 -> nsc2`; reemplazo de `GenerationBuilder`, sin ejecutar `nsc0`, sin image replay y con auditoría del árbol de procesos. |
| 44 | SH42 | Equivalencia funcional `nsc0`/`nsc2`: diagnósticos, AST, semántica, IR, objetos, metadata, productos y proyectos. |
| 45 | SH43 | Punto fijo `nsc2 -> nsc3`, repetido tres veces desde limpio, con identidad conforme a la política y sin normalización oculta. |
| 46 | SH44 | Determinismo entre dos máquinas: rutas, espacios, Unicode, locales, zona horaria, workers y orden del sistema de archivos. |
| 47 | SH45 | Paridad low-level 2.0: volatile, atomics, unions, bitfields, layout, placement/destrucción, noheap y escape analysis contractual. |
| 48 | SH46 | ABI e interop completos: C, C++, COM, callbacks, aggregates, varargs, vectorcall y DLL; cualquier cambio de NSLink vive y se certifica en su repositorio. |
| 49 | SH47 | SIMD y código especializado: vectores, matrices, half, packed, intrinsics, inline asm y modos floating point con NSAsm y ambos linkers. |
| 50 | SH48 | Biblioteca y runtime completos reconstruidos: threading, atomics, sincronización, jobs, IO, filesystem, procesos, diagnósticos y runtime Win32. |
| 51 | SH49 | Toolchain avanzado: comptime sandboxed, caché reproducible, sanitizer, SDK, metadata N/N-1, paquetes y bindings precompilados. |
| 52 | SH50 | Rendimiento: O0 con presupuesto estricto de latencia, O1–O3 efectivos, incremental, caché, scheduling, paralelismo, LTO y ThinLTO medidos. |
| 53 | SH51 | Experiencia de desarrollo: progreso, verbosidad, logs persistentes, errores accionables, PDB O0/optimizado/inline, Visual Studio y contrato del NativeSharp Debugger. |
| 54 | SH52 | Robustez: fuzzing, metadata hostil, cancelación, fallo atómico, interrupción, limpieza de temporales, cero messageboxes y reproducibilidad. |
| 55 | SH53 | Distribución: instalación, actualización, convivencia, rollback, migración, congelamiento de `nsc0` y receta de recuperación comprobada. |
| 56 | SH54 | Certificación final: matriz completa desde limpio, cero omitidos y cero incidencias de release, acta, hashes, milestone/tag 2.0 y retorno a `development`. |

#### Regla terminal

La tanda 43 produce el primer compilador que se compila realmente a sí mismo, pero no constituye la
promoción final. La tanda 56 es el cierre terminal de NativeSharp 2.0 self-hosted: cualquier defecto
descubierto por SH54 se corrige y vuelve a validar dentro de la tanda 56; no se difiere a una tanda 57
ni se presenta después como requisito imprevisto. Terminado SH54, el siguiente trabajo planificado
pertenece a NativeSharp 3.0 y debe tener su propio roadmap.

El C++ de `nsc0` continúa permitido únicamente como semilla y recuperación hasta SH53; no se
reescribe LLVM. Si NSLink o NSAsm requieren implementación nueva en NativeSharp, esa implementación
se crea desde cero en repositorios independientes y nunca se mezcla con el código de `nsc-next`.

#### Método obligatorio para T29–T56

Cada tanda conserva el método de la sección 22: baseline de entrada, cambio acotado, pruebas
dirigidas O0–O3 con LLD y NSLink, baseline completo de cierre, evidencia y hashes, expediente técnico
con métodos/funciones/archivos/diseño y un único commit `selfhost(tanda-XX): ...`. Durante estas
tandas la instalación de trabajo se limita a `A:\native-sharp`; las rutas de scripts deben ser
configurables y ningún commit puede contener objetos, ejecutables, temporales ni archivos ajenos. No
se cierra un gate con una prueba obligatoria omitida o con una incidencia abierta de severidad de
release.

## 9. Orden de dependencias

```text
SH00 punto de partida
  └─ SH01 perfil bootstrap
      ├─ SH02 biblioteca anfitriona
      └─ SH03 LLVM/runtime/plataforma
          └─ SH04 esqueleto
              └─ SH05 lexer/parser
                  └─ SH06 semántica
                      └─ SH07 IR
                          └─ SH08 backend
                              └─ SH09 driver/proyectos
                                  └─ SH10 nsc1
                                      └─ SH11 nsc2
                                          └─ SH12 nsc3/punto fijo
                                              └─ SH13 robustez
                                                  └─ SH14 promoción condicionada
                                                      └─ SH15 retiro de image replay
                                                          └─ SH16 backend derivado de fuente
                                                              └─ SH17 funciones/parámetros/llamadas
                                                                  └─ SH18 locales/control de flujo
                                                                      └─ SH19 control de bucles
                                                                          └─ SH20 escalares bool/void
                                                                              └─ SH21 expresiones escalares
                                                                                  └─ SH22 anchos enteros
                                                                                      └─ SH23 punteros/memoria local
                                                                                          └─ SH24 escalares flotantes
```

La continuación normativa es secuencial a nivel de gate:

```text
SH24 → SH25 → SH26 → SH27 → SH28 → SH29 → SH30 → SH31 → SH32 → SH33 → SH34
     → SH35 → SH36 → SH37 → SH38 → SH39 → SH40
     → SH41 → SH42 → SH43 → SH44
     → SH45 → SH46 → SH47 → SH48 → SH49
     → SH50 → SH51 → SH52 → SH53 → SH54
```

SH25–SH40 completan compilador y cierre bootstrap; SH41–SH44 prueban generaciones reales y
determinismo; SH45–SH49 cierran la superficie 2.0 y su runtime; SH50–SH54 cierran rendimiento,
diagnósticos, robustez, distribución y promoción.

SH02 y SH03 pueden avanzar en paralelo después de SH01. Las demás dependencias son secuenciales por
corrección, aunque las tareas internas de una fase pueden paralelizarse si no comparten contratos.

## 10. Estrategia de port

No se portarán 191 archivos C++ en bloque. La unidad de avance será una rebanada vertical que pueda
compararse de extremo a extremo:

1. Seleccionar un comportamiento observable y su corpus.
2. Escribir o congelar el contrato canónico.
3. Implementarlo en NativeSharp usando sólo el Bootstrap Profile.
4. Emitir un dump estructurado estable.
5. Compararlo contra `nsc0`.
6. Reducir cualquier divergencia a una fixture mínima.
7. Integrar el subsistema y ejecutar los gates acumulados.

Las clases C++ pueden inspirar el comportamiento, pero no obligan a reproducir su ownership, sus
contenedores ni su jerarquía. Los formatos y efectos observables sí deben preservarse.

## 11. Matriz mínima de pruebas

| Dimensión | Valores obligatorios antes de promoción |
|---|---|
| Generación | nsc0, nsc1, nsc2, nsc3 |
| Linker | lld-link, NSLink |
| Optimización | O0, O1, O2, O3 |
| Producto | EXE, LIB, DLL |
| Entrada | fuente única, proyecto, referencias, precompilados, SDK |
| Debug | sin PDB, PDB O0, PDB optimizado, inline |
| ABI | NativeSharp, C, C++, COM, callbacks, varargs, vectorcall |
| Runtime | ownership, EH, threading, atomics, IO, filesystem, procesos |
| Metadata | nsmeta actual, N-1 compatible, corrupta, futura rechazada |
| Estado | limpio, incremental, caché fría, caché caliente, cancelado |
| Rutas | corta, larga, espacios, Unicode, distinto volumen |
| Entorno | locale/timezone controlados, número de workers variable |

Cada celda aplicable debe declarar número de casos, aprobados, fallidos y omitidos. Un omitido sin
excepción aprobada cuenta como fallo del gate.

## 12. Harness diferencial

El harness debe ejecutar ambos compiladores con entradas idénticas y comparar por capas:

1. códigos de salida;
2. diagnósticos estructurados y orden;
3. tokens y árbol sintáctico canónicos;
4. símbolos, tipos, layouts y especializaciones;
5. HIR, MIR, CFG y SSA canónicos;
6. LLVM IR normalizado;
7. símbolos, secciones, relocations, exports y unwind de objetos;
8. `nsmeta`, manifiestos y contratos;
9. salida y comportamiento del ejecutable;
10. PDB/CodeView mediante verificadores estructurales.

Las rutas absolutas, timestamps y build IDs se sustituyen sólo durante comparación y sólo si el
contrato los declara no semánticos. El artefacto original nunca se modifica.

## 13. Orquestación prevista

La integración debe extender las herramientas reales del proyecto. Las interfaces siguientes son
objetivos por implementar, no comandos disponibles en el corte inicial:

```text
python nscbuilder.py --selfhost --stage nsc1
python nscbuilder.py --selfhost --stage nsc2
python nscbuilder.py --selfhost --stage nsc3
python nscbuilder.py --selfhost --verify-fixed-point
python nscbuilder.py --selfhost --full-matrix --jobs N
```

La orquestación debe:

- resolver binarios mediante rutas absolutas y manifiestos, nunca sólo mediante `PATH`;
- registrar cada comando sin exponer secretos;
- crear un directorio nuevo por corrida;
- impedir mezcla de objetos entre generaciones;
- verificar hashes antes y después de cada etapa;
- fallar al encontrar artefactos inesperados;
- conservar logs de la primera diferencia;
- publicar resumen Markdown y reporte JSON;
- no instalar nada hasta que el gate correspondiente haya pasado.

## 14. Política de determinismo

Toda colección cuya iteración afecte salida debe tener orden explícito. Los siguientes valores no
pueden entrar accidentalmente a un artefacto reproducible:

- dirección de memoria;
- orden de threads;
- ruta absoluta del checkout;
- hora local o timezone;
- locale del proceso;
- enumeración no ordenada del filesystem;
- semilla aleatoria implícita;
- identificadores incrementales dependientes del scheduling;
- timestamp no normalizado;
- versión encontrada casualmente en `PATH`.

Los IDs semánticos se derivarán de claves estables o de recorridos canónicos. El paralelismo debe
cambiar el tiempo, nunca la salida.

## 15. Contrato de equivalencia `nsc2`/`nsc3`

La comparación tiene tres niveles:

### Nivel A — identidad obligatoria

- diagnósticos estructurados;
- archivos `nsmeta`;
- contratos y manifests;
- hashes de fuentes y dependencias;
- LLVM IR canónico;
- listas de símbolos, exports y dependencias;
- runtime y bibliotecas inmutables consumidas.

### Nivel B — identidad binaria objetivo

- `nsc2.exe` contra `nsc3.exe`;
- objetos del compilador;
- bibliotecas estáticas;
- recursos y archivos auxiliares.

Si no son idénticos, el gate permanece abierto hasta demostrar y eliminar la fuente de variación o
trasladarla explícitamente al Nivel C mediante una decisión versionada.

### Nivel C — equivalencia estructural excepcional

Sólo se admite para campos cuyo formato externo obligue a variar y que el verificador pueda aislar,
por ejemplo un GUID o timestamp de depuración. La excepción debe incluir:

- nombre exacto del campo;
- razón por la que no puede estabilizarse todavía;
- normalización usada únicamente para comparar;
- prueba de que no cambia ejecución, ABI ni depuración;
- responsable y fecha de eliminación prevista.

No se permite declarar equivalentes dos ejecutables únicamente porque pasan pruebas.

## 16. Gates y política de fallos

Cada gate conserva:

- revisión Git y estado del árbol;
- identidad de todas las herramientas;
- comandos y códigos de salida;
- conteos de pruebas;
- hashes de entradas y salidas;
- problemas conocidos;
- estado `OPEN`, `LANDED`, `REJECTED` o `SUPERSEDED`;
- enlace al gate anterior.

Una fase fallida no instala ni promociona artefactos. Se conserva el primer fallo, se reduce y se
repite desde una salida limpia. Nunca se maquilla un fallo copiando binarios de otra corrida.

## 17. Riesgos principales

| Riesgo | Señal temprana | Mitigación |
|---|---|---|
| Perfil circular | nsc-next necesita una feature que sólo él compilaría | verificador SH01 obligatorio |
| Binding LLVM incorrecto | crashes o IR inválido lejos del origen | smoke tests por símbolo y versión |
| Divergencia silenciosa | ejecuta bien pero cambia ABI/nsmeta | comparación por capas y matrices ABI |
| Port monolítico | semanas sin ejecutable comparable | rebanadas verticales y gates acumulativos |
| Orden no determinista | hashes cambian con jobs/ruta | determinismo desde Basic y pruebas cruzadas |
| Runtime todavía generado por C++ | nsc2 depende ocultamente de nsc0 | runtime versionado y process-tree auditado |
| Uso accidental de instalación global | sólo pasa en una máquina | rutas absolutas, lockfile y árbol limpio |
| Rendimiento prematuro | optimizaciones esconden errores | corrección primero; presupuesto en SH13 |
| Doble implementación indefinida | fixes sólo llegan a un compilador | feature freeze de nsc0 al entrar en RC |
| Pérdida de recuperación | nsc-next roto bloquea el proyecto | semilla congelada y receta reproducible |

## 18. Política de compatibilidad durante la transición

1. `nsc0` y `nsc-next` comparten la misma versión de lenguaje 2.0 y el mismo runtime ABI.
2. Una extensión necesaria para self-hosting se implementa primero como cambio normal de 2.0 con
   especificación y baseline; después puede entrar al Bootstrap Profile.
3. `nsc-next` debe consumir bibliotecas y `nsmeta` producidos por `nsc0`, y viceversa mientras ambos
   estén soportados.
4. Los proyectos consumidores no deben necesitar cambios para elegir el compilador.
5. La CLI nueva no elimina opciones existentes antes de la ventana de compatibilidad.
6. Los diagnósticos conservan código y significado; el texto puede mejorar sólo con expediente.
7. La instalación RC mantiene binarios lado a lado y selección explícita.

## 19. Criterio de promoción final

NativeSharp 2.0 puede declararse self-hosted cuando todos estos puntos sean verdaderos al mismo
tiempo:

- [ ] Gates SH00–SH54 están `LANDED` y enlazados; ningún gate histórico dependiente de image replay
      conserva valor de promoción sin su revalidación en SH41–SH54.
- [ ] El Bootstrap Profile está congelado, versionado y verificado.
- [ ] `nsc1`, `nsc2` y `nsc3` se producen desde árboles limpios.
- [ ] `nsc2` se construyó sin ejecutar `nsc0`.
- [ ] `nsc2` y `nsc3` alcanzan el punto fijo definido.
- [ ] La repetición en dos máquinas no introduce diferencias no autorizadas.
- [ ] Core, Collections, IO y LLVM.C fueron reconstruidas desde fuente sin objeto oculto de `nsc0`.
- [ ] CTest completo y toda prueba acumulada pasan con ambos linkers.
- [ ] Fixtures y triage pasan en O0/O1/O2/O3.
- [ ] La superficie completa del lenguaje 2.0, biblioteca estándar y runtime está certificada.
- [ ] ABI, DLL, COM, PDB, LTO, metadata, paquetes y bindings están certificados.
- [ ] O0 cumple su presupuesto de compilación y O1/O2/O3 producen optimización real sin divergencia
      semántica.
- [ ] Progreso, verbosidad, logs, diagnósticos, Visual Studio y NativeSharp Debugger cumplen sus
      contratos publicados.
- [ ] NSLink y NSAsm son compatibles; cualquier cambio o reescritura está trazado en su repositorio
      independiente y enlazado desde la evidencia.
- [ ] Cero pruebas obligatorias omitidas.
- [ ] Cero problemas abiertos de severidad de release.
- [ ] Instalación, actualización, convivencia y rollback están probados.
- [ ] La documentación y los manifiestos corresponden al binario promovido.
- [ ] `nsc0` está congelado y existe una receta comprobada para recuperarlo.
- [ ] El acta de promoción identifica revisión, hashes y reportes exactos.

Un primer `nsc1.exe` que compila ejemplos no satisface esta definición. Tampoco la satisface un
`nsc2` funcional si `nsc2` y `nsc3` divergen sin explicación. Si la matriz final descubre una brecha,
SH54 permanece abierto hasta corregirla: no se crea una tanda posterior para declarar un cierre
incompleto.

## 20. Primeras tandas ejecutables

El trabajo inmediato se divide en dos commits verificables antes de portar código:

### Tanda 01 — SH00

1. crear el layout `selfhost/`;
2. fijar nombres y aislamiento de `nsc0`, `nsc1`, `nsc2` y `nsc3`;
3. inventariar dependencias externas permitidas;
4. definir `bootstrap.lock` y su schema;
5. crear el verificador de entorno;
6. hacer configurables prefijo, builds, LLVM, linkers, trabajo, temporales y evidencia;
7. validar raíces de escritura antes de ejecutar;
8. crear limpieza dirigida y recuperación mediante `nsc0`;
9. definir formatos de reporte y gate;
10. ejecutar el baseline normal y registrar Gate SH00.

### Tanda 02 — SH01

1. escribir `BootstrapProfile-2.0.md`;
2. generar el inventario de construcciones usadas por el esqueleto previsto;
3. fijar el subconjunto inicial de stdlib;
4. crear el verificador de perfil;
5. crear fixtures positivas y negativas del perfil;
6. ejecutar el baseline normal y registrar Gate SH01.

No debe comenzar el port del lexer hasta que ambas tandas estén certificadas. Esto evita descubrir
a mitad del compilador que una construcción fundamental no pertenece al subconjunto bootstrap.

## 21. Mantenimiento de este roadmap

- Este archivo es el plan vigente para self-hosting de NativeSharp 2.0.
- Todo cambio de alcance, dependencia, gate o equivalencia se registra aquí en el mismo commit que
  cambia el contrato correspondiente.
- El cierre de una tarea sustituye su estado planeado por enlace a evidencia; no borra el criterio.
- Los documentos históricos pueden explicar decisiones, pero no se requieren para entender el plan.
- Una eventual edición NativeSharp 3.0 copiará y revisará el plan; no modificará retroactivamente el
  contrato de 2.0.

## 22. Expediente obligatorio por tanda y commit

El trabajo se divide en tandas numeradas. Cada tanda se cierra con un commit identificable y el
mismo commit debe incluir un expediente Markdown bajo `docs/2.0/self-hosting/tandas/`. No se acepta
un commit de cierre que cambie código, contratos, pruebas u orquestación sin actualizar su
expediente. Una corrección posterior de una tanda cerrada debe actualizar el mismo expediente en el
commit correctivo y enlazar la evidencia nueva sin borrar la anterior.

El expediente de cada tanda debe contener como mínimo:

1. número, nombre, estado, tareas `NS2-SH-xxx`, gate y alcance;
2. commit de entrada, rama de trabajo, commit de cierre y relación con el milestone;
3. objetivo, no objetivos, contratos, invariantes y decisiones de diseño;
4. arquitectura y flujo de datos de la rebanada implementada;
5. inventario de cada archivo creado, modificado o eliminado y la razón del cambio;
6. inventario de tipos, clases, structs, interfaces, enums y aliases introducidos o modificados;
7. inventario de métodos y funciones con firma, responsabilidad, entradas, salidas, errores,
   ownership, efectos laterales y requisitos de concurrencia aplicables;
8. formatos, opciones de CLI, variables de entorno, códigos de diagnóstico y versiones de contrato;
9. pruebas añadidas o modificadas, propósito de cada una y cobertura positiva, negativa y de
   regresión;
10. comandos exactos de validación y sus resultados;
11. baseline completo de la tanda, con identificador de corrida, revisión, hashes, conteos,
    linkers, perfiles de optimización, problemas y rutas de los reportes;
12. diferencias contra `nsc0` o contra la generación anterior y su resolución;
13. riesgos, deuda aceptada, limitaciones y trabajo que queda explícitamente fuera;
14. lista final de archivos del commit y comprobación de que no contiene objetos, ejecutables,
    temporales ni artefactos ajenos.

La plantilla normativa es `docs/2.0/self-hosting/TANDA_TEMPLATE.md`. Puede ampliarse, pero ninguna
sección obligatoria puede eliminarse o marcarse como no aplicable sin justificarlo. Los reportes
voluminosos y datos estructurados viven en el directorio de evidencia de la corrida; el Markdown
los enlaza mediante rutas estables y registra sus hashes.

### 22.1 Protocolo de baseline por tanda

Cada tanda tiene exactamente un baseline de cierre obligatorio, ejecutado después del último cambio
funcional y antes del commit de cierre. Se usa el comando canónico:

```text
python nscbuilder.py --baseline --jobs N
```

Las rutas no forman parte del código del script. Pueden resolverse por CLI, entorno o un archivo
`nscbuilder-paths` v1; una corrida aislada puede añadir `--write-root` para impedir físicamente que
el runner escriba fuera de las unidades o directorios autorizados.

Las generaciones disponibles en esa tanda añaden sus matrices acumulativas al baseline normal. Un
baseline fallido no se resume como aprobado: se conserva el primer fallo, se corrige, se repite la
corrida completa y se documentan ambas ejecuciones. El expediente identifica únicamente como
baseline de cierre una corrida completamente verde y conserva las fallidas como antecedentes.

El commit de cierre sigue el formato:

```text
selfhost(tanda-XX): descripcion breve
```

Antes de cerrarlo se verifica el diff por rutas y se actualiza el expediente con el hash de la
evidencia. El expediente identifica el cierre como `este commit`; el SHA exacto se obtiene del
propio historial Git, porque un commit no puede contener su identificador sin modificarlo. Así se
mantiene un solo commit de cierre por tanda.

## 23. Resumen ejecutivo

Las tandas 01–41 y los gates SH00–SH39 ya establecieron contratos, retiraron image replay, lograron
un backend derivado de fuente para la rebanada escalar actual, construyeron el grafo fuente
determinista, dieron identidad semántica al conjunto multimódulo y cerraron la memoria nativa que
usa el bootstrap. Eso es una base real, no self-hosting completo: todavía faltan interop extendido, genéricos,
el resto de la superficie 2.0, bibliotecas, generaciones autónomas, punto fijo y certificación.

El plan cerrado restante comprende las tandas 42–56 y los gates SH40–SH54. SH41, en la tanda 43,
produce la primera autocompilación real; SH54, en la tanda 56, es el único cierre de promoción. El
roadmap contiene así 56 tandas y 55 gates acumulativos (`SH00`–`SH54`), con baseline, expediente y
commit por tanda. Después de SH54 no queda una fase de self-hosting 2.0 por descubrir: el siguiente
roadmap es NativeSharp 3.0.
