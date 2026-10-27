# NSC — Auditoría técnica de LTO

**Fecha:** 2026-10-24
**Rama auditada:** `development-release-candidate`
**HEAD auditado:** `871640d9e30f66f4d0c0288c3270ebf79c438617`
**Naturaleza:** auditoría estática; no es implementación ni certificación de rendimiento

## 1. Alcance

Esta auditoría determina qué significa actualmente LTO en NSC, cuánto de la superficie existente produce una
optimización real, qué falta para alcanzar un modelo comparable al LTO de GCC/Clang y cuál es el orden técnico
recomendado para implementarlo sin modificar sintaxis, ABI, NSMETA ni contratos congelados del lenguaje.

No se compiló, enlazó ni ejecutó ninguna prueba. No se hicieron baselines ni se modificó código fuente.

Durante la redacción, HEAD avanzó desde `597013e` mediante seis commits concurrentes de corrección semántica y
debug/codegen. Se auditó su diferencial: no modifica la superficie ni la implementación LTO descrita aquí.

## 2. Veredicto

| Área | Estado | Veredicto |
|---|---|---|
| `-flto` / `-flto=full` | PARCIAL REAL | Consolida el proyecto en un módulo LLVM y permite optimización interprocedural dentro de ese proyecto. |
| `-flto=thin` | SUPERFICIE SIN THINLTO REAL | Produce particiones y un digest estable, pero no ejecuta resumen, importación ni optimización ThinLTO. |
| LTO entre bibliotecas | NO EXISTE | Las bibliotecas llegan al enlace como objetos COFF; no se conserva IR para optimización cruzada. |
| LTO desde `nsc build` | NO EXPUESTO | El comando interno lo acepta, pero `nsc build` no lo modela ni lo reenvía. |
| Integración con NSLink | NO EXISTE | NSLink recibe objetos nativos, no bitcode ni índices de ThinLTO. |
| Pruebas de beneficio | INSUFICIENTES | Las pruebas comprueban aceptación, digest y ejecución, no inlining o mejora medible. |

Conclusión: NSC ya tiene una modalidad útil de optimización global intraproyecto denominada `full`, pero todavía
no tiene LTO equivalente al de GCC/Clang para una cadena completa de aplicaciones y bibliotecas. La modalidad
actual `thin` es telemetría y particionado; no debe describirse como una implementación real de ThinLTO.

## 3. Superficie pública existente

### 3.1 Modos

`include/nativesharp/LLVM/LinkTimeOptimizationMode.h` declara:

- `None`
- `Full`
- `Thin`

`src/Driver/CompilerInvocation.h` conserva `None` como valor predeterminado.

`src/Driver/CompilerOptionParser.cpp` reconoce:

- `-flto`
- `-flto=full`
- `-flto=thin`

La validación del driver permite la opción en las acciones internas de emisión OOP de ejecutable, biblioteca y
DLL.

### 3.2 Falta de integración en `nsc build`

`src/Driver/ProjectCommands.cpp` no declara una preferencia de LTO ni interpreta una opción equivalente durante
`nsc build`. `appendNestedCompilerOptions` tampoco la reenvía a la invocación interna del compilador.

Consecuencia: la superficie existe para invocaciones internas/directas, pero no forma parte del flujo normal de
construcción de proyectos. Antes de promover LTO como capacidad de producto debe existir una única política de
configuración, propagación y diagnóstico.

## 4. Auditoría de `full`

### 4.1 Qué hace realmente

En `src/LLVM/ProjectOopObjectGenerator.cpp`, `Full` desactiva el uso de particiones. El lowerer recibe entonces las
funciones MIR del proyecto en un solo módulo LLVM. Posteriormente `src/LLVM/LlvmOptimizer.cpp` aplica el pipeline
ordinario de LLVM correspondiente a O0, O1, O2 u O3 y emite un objeto nativo.

Esto sí permite que los pases ordinarios de LLVM vean el programa completo del proyecto y realicen optimizaciones
interprocedurales que no podrían cruzar módulos particionados.

### 4.2 Qué no hace

No se localizaron usos de:

- `llvm::lto`
- `ThinLTOCodeGenerator`
- `ModuleSummaryIndex`
- generación o consumo de bitcode para el enlazador
- pipelines LTO pre-link/post-link
- importación de cuerpos entre módulos

NSC termina produciendo un objeto nativo y NSLink enlaza objetos COFF. Por tanto, `full` no cruza límites de
bibliotecas precompiladas ni ejecuta una fase de optimización dirigida por el enlazador.

### 4.3 Costos y límites

- Pierde paralelismo entre unidades LLVM al obligar a usar un solo módulo.
- Aumenta la memoria máxima del compilador.
- En O0 ofrece poco o ningún beneficio de optimización.
- En configuraciones de depuración optimizada el pipeline ya puede quedar monolítico por requisitos de scopes e
  inlining; en esos casos `full` puede no cambiar materialmente la forma de trabajo.
- La caché de objetos incluye el modo LTO en su identidad, lo cual es correcto, pero el cambio de modalidad crea
  un dominio de caché distinto.

## 5. Auditoría de `thin`

### 5.1 Qué hace actualmente

El generador conserva las particiones, optimiza y emite cada módulo por separado. Después calcula
`thinLtoIndexDigest` a partir del texto de los módulos ya optimizados, ordenado por ordinal, y publica ese digest
como resultado/telemetría.

Las pruebas verifican que existan varias unidades o resúmenes, que el digest tenga formato estable y que el
ejecutable resultante funcione.

### 5.2 Qué falta para que sea ThinLTO

El digest no se consume para transformar código. No existe:

- resumen global de símbolos y call graph;
- resolución de prevailing symbols;
- selección de exportaciones/importaciones;
- importación selectiva de cuerpos;
- internalización global;
- promoción de símbolos locales;
- optimización posterior a importación;
- caché de backends ThinLTO por índice/import set;
- coordinación de índices entre aplicación y bibliotecas.

Además, el modo `Thin` actual desactiva la caché pre-LLVM y la caché pre-optimización, y fuerza la preservación de
IR textual. Puede, por tanto, costar más que el modo normal sin entregar los beneficios que justifican ese costo.

### 5.3 Clasificación correcta

El nombre público ya está congelado y no se propone cambiarlo en esta auditoría. Internamente, sin embargo, el
estado debe tratarse como `ThinLTO pendiente`, no como una implementación terminada.

## 6. Cobertura de pruebas existente

`tests/unit/Item390LtoTests.cpp` comprueba, para `full`:

- que la compilación termina;
- que la modalidad se informa;
- que existe una sola unidad de código;
- que el ejecutable devuelve el valor esperado.

`tests/unit/Item391ThinLtoTests.cpp` comprueba, para `thin`:

- que la compilación termina;
- que se reportan múltiples resúmenes/unidades;
- que el digest tiene 64 caracteres y es estable entre dos ejecuciones;
- que el ejecutable devuelve el valor esperado.

La cobertura actual no demuestra:

- inlining cruzando módulos o bibliotecas;
- eliminación global de código;
- internalización;
- igualdad funcional en DLL y bibliotecas estáticas;
- mejora de tamaño, tiempo de ejecución o tiempo de enlace;
- aciertos de una caché ThinLTO real.

## 7. Diferencia frente a GCC/Clang

En una implementación LTO convencional, los objetos conservan IR/bitcode y el enlazador participa en una
resolución global. Full LTO combina el programa durante el enlace. ThinLTO conserva backends paralelos, construye
un índice global compacto e importa únicamente cuerpos rentables.

NSC `full` se parece a una compilación monolítica de todo el proyecto antes del enlace. Es útil, pero su frontera
termina donde termina el proyecto fuente actual. NSC `thin` todavía no realiza el protocolo ThinLTO.

## 8. Arquitectura recomendada

### LTO-01 — Exponer la política en `nsc build`

Agregar una preferencia de proyecto/build que reenvíe exactamente `none`, `full` o `thin` a la invocación interna.
Debe formar parte de la identidad del build y de los diagnósticos concisos. No implica cambiar el lenguaje.

### LTO-02 — Implementar ThinLTO intraproyecto real

Reemplazar el digest de texto como centro de la modalidad por las APIs C++ de LLVM para:

1. emitir bitcode con summary por partición;
2. combinar summaries en un índice global;
3. resolver símbolos y calcular imports/exports;
4. ejecutar los backends ThinLTO en paralelo;
5. cachear cada backend por módulo, índice, import set, target, opciones y versión de LLVM;
6. emitir objetos COFF normales para NSLink.

Esta fase entrega valor sin exigir todavía cambios en el formato de bibliotecas NativeSharp.

### LTO-03 — Extender LTO a bibliotecas NativeSharp

Una biblioteca que quiera participar debe publicar un sidecar de bitcode/summary, ligado criptográficamente a:

- NSMETA;
- objeto o biblioteca física;
- target triple y data layout;
- versión/identidad de LLVM;
- opciones ABI y de codegen;
- build hash del productor.

El consumidor debe rechazar el sidecar ante cualquier desacuerdo y conservar el objeto nativo como fallback.
NSMETA no se versiona de nuevo ni se altera su contrato para lograrlo.

### LTO-04 — Coordinar el enlace

LLVM debe seguir siendo responsabilidad de NSC o de un helper controlado por NSC. NSLink puede coordinar entradas
y resolución, pero no debe duplicar optimizadores. La salida hacia NSLink puede seguir siendo COFF una vez
terminados los backends LTO.

### LTO-05 — Certificar beneficio y equivalencia

Se requieren casos focales que prueben transformaciones observables en IR y un baseline separado para:

- tiempo de compilación;
- memoria máxima;
- tamaño de binario;
- tiempo de ejecución;
- caché fría y caliente;
- aplicación, biblioteca estática y DLL;
- O1, O2 y O3.

## 9. Riesgos que deben bloquear una implementación incorrecta

- Usar texto LLVM como formato estable o como índice ThinLTO.
- Mezclar bitcode producido por versiones o targets incompatibles.
- Permitir que una caché LTO ignore NSMETA, target, imports o herramientas de enlace.
- Desactivar las cachés existentes sin que la modalidad entregue optimización global real.
- Considerar que un digest estable demuestra ThinLTO.
- Hacer que el éxito dependa de que todas las bibliotecas tengan bitcode; debe existir fallback nativo.

## 10. Orden recomendado

1. Integrar la opción en `nsc build` y cerrar la identidad/configuración.
2. Conservar `full` como optimización monolítica intraproyecto y medir su costo/beneficio real.
3. Implementar ThinLTO real dentro de un proyecto.
4. Añadir caché de backends ThinLTO.
5. Diseñar y congelar el sidecar de bibliotecas.
6. Extender la optimización a dependencias que publiquen sidecar compatible.
7. Certificar rendimiento y equivalencia por configuración.

No se recomienda priorizar LTO antes de cerrar la reutilización incremental del frontend: LTO mejora principalmente
el binario producido y puede encarecer la construcción, mientras que el problema principal de latencia interactiva
continúa estando en el trabajo semántico que se repite después de editar una fuente.

## 11. Estado final: LTO congelado

**Fecha:** 2026-10-25
**Rama:** `development-release-candidate`
**Puntos del plan:** 41 a 55 de `NSC-PLAN-OPTIMIZACION-PARTE-2.md`

Esta sección no reescribe la auditoría anterior: registra qué quedó implementado sobre ella. Las cifras de tiempo,
memoria y tamaño se registran sólo en `NSC-MEDICION-PARTE-2.md`.

LTO queda **congelado**: implementado y validado. Cualquier cambio posterior a ThinLTO, al sidecar o a su identidad
es un trabajo nuevo con su propia auditoría, no una continuación de este plan.

### 11.1 Veredicto actualizado

| Área | Estado al auditar | Estado final | Commits |
|---|---|---|---|
| `-flto` / `-flto=full` | PARCIAL REAL | Sin cambios: módulo monolítico intraproyecto. | — |
| `-flto=thin` en `-O1`–`-O3` | SUPERFICIE SIN THINLTO REAL | ThinLTO REAL intraproyecto (sección 11.2). | `83b0aaef4` |
| `-flto=thin` en `-O0` | Superficie | Build particionado normal, con aviso bilingüe. | `83b0aaef4` |
| Caché de backends ThinLTO | NO EXISTE | Por módulo, índice, imports, target, opciones y LLVM. | `ebd178263` |
| ThinLTO con `-g` | — | Reproducible: objetos sin ruta de objeto en CodeView. | `6563c2f6c` |
| Internalización y definiciones duplicadas | NO EXISTE | HECHO: internaliza lo que ningún objeto nativo pide y el enlace descarta secciones sin referencia. | `adaf21e5a` |
| Estabilidad de `-g` ante ediciones | — | HECHO: una edición sólo invalida las unidades que leen el archivo editado. | `5cb347f25` |
| LTO entre bibliotecas | NO EXISTE | Estáticas, mediante el sidecar `.nsthinlto` v2; nunca entre DLL. | `36a99d973`, `7c4d6f58d`, `6286ae820` |
| Sidecar en `nsc restore` | — | HECHO: se restaura con su paquete estático y entra al lock. | `0bac53813` |
| LTO desde `nsc build` | NO EXPUESTO | `-flto` con sus tres modos y `link_time_optimization`. | `58d063e82`, `9debff0c3` |
| LTO desde el IDE | — | Campo `-flto` en la configuración de build. | IDE `4cf2ca6` |
| Integración con NSLink | NO EXISTE | Sólo `/opt:ref` cuando hubo internalización; NSC corre LLVM y NSLink recibe COFF. | `adaf21e5a` |
| Pruebas de beneficio | INSUFICIENTES | Pruebas unitarias y matriz de línea de comandos (sección 11.5). | ver 11.5 |

### 11.2 ThinLTO intraproyecto

`ThinLtoBackend.cpp` es el único archivo del compilador que usa la API C++ de LLVM (`llvm::lto`). Cada partición
pasa por `thinlto-pre-link<On>` y se escribe como bitcode con su resumen y su hash de módulo; `llvm::lto::LTO`
arma el índice combinado, resuelve símbolos, calcula imports y exports y ejecuta un backend por unidad en paralelo.
Los objetos resultantes reciben rangos de procedimientos CodeView y verificación COFF como cualquier otro.

Respecto de la lista de la sección 5.2:

| Pieza | Estado |
|---|---|
| Resumen global y grafo de llamadas | HECHO (resumen de módulo por unidad e índice combinado de LLVM). |
| Resolución de símbolos prevalecientes | HECHO: prevalece la primera definición. |
| Selección de imports y exports | HECHO (LLVM). |
| Importación selectiva de cuerpos | HECHO (LLVM). |
| Internalización global | HECHO (ver abajo). |
| Promoción de símbolos locales | HECHO (LLVM, al importar). |
| Optimización posterior a la importación | HECHO (pipeline de post-enlace del backend). |
| Caché de backends | HECHO (`<caché>/thinlto-backends/<identidad de codegen>`, poda de una semana y 1 GiB). |
| Coordinación entre aplicación y bibliotecas | HECHO para bibliotecas estáticas con sidecar compatible. |

**Internalización.** Un símbolo queda visible para objetos nativos (`VisibleToRegularObj`) sólo si lo preserva el
build: el punto de entrada o los exports de la DLL, y todo símbolo indefinido del runtime configurado y de las
bibliotecas de enlace que no se consumieron por sidecar, leídos con `llvm::object`. Lo demás se internaliza; LLVM
elimina lo que quedó sin uso, los objetos se emiten con secciones por función y por dato, y el enlace recibe
`/opt:ref`. La caché de backends internalizados vive en el subdirectorio `internalized`. No se internaliza al
producir bibliotecas estáticas, cuando el runtime no es una biblioteca aparte ni cuando alguna entrada no se puede
leer: en esos casos todo símbolo queda visible, como antes. Con esto desaparece el crecimiento del binario que la
versión sin internalización tenía respecto del build particionado.

**Estabilidad de `-g`.** `SourceManager::getFile` avisa a un registro por hilo (`SourceAccessRecording`) qué archivos
lee el lowering de cada unidad. La llave del objeto previo a LLVM es la de la unidad más ruta, tamaño y SHA-256 de
esos archivos (`SourceDependencyIndex`), y el manifiesto de archivos se guarda junto con el plan de particiones. La
identidad global de tabla de líneas se reemplazó por la del mapa de prefijos de depuración, y la identidad del
modelo de depuración completa ya no incluye datos derivados de los cuerpos (vínculos de búsqueda de nombres y
efectos inferidos). Una edición sólo vuelve a bajar a LLVM las unidades que leen el archivo editado, con `-g` y con
`-gline-tables-only`. Al hacerlo apareció y se corrigió un defecto previo: un sitio de pánico conservaba su línea
vieja en un build con caché.

En un host DYNAMIC (sólo `LLVM-C.dll`) no hay `llvm::lto`; `-flto=thin` conserva allí el build particionado.

### 11.3 Bibliotecas estáticas

El contrato está en `NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md`. Una biblioteca estática con ThinLTO efectivo y
runtime aparte publica `<Biblioteca>.nsthinlto`: bitcode de pre-enlace de cada unidad, identidad de compatibilidad
y SHA-256 de la `.lib` y la `.nsmeta`. Un ejecutable o una DLL con ThinLTO efectivo lo consume sólo si todo
coincide byte a byte; en otro caso enlaza la `.lib` nativa e informa `thinlto-library-fallback` con su motivo. Las
unidades consumidas se enlazan como un archivo COFF de reemplazo con ruta direccionada por contenido.

El formato v2 agrega a la identidad:

- la `.nsmeta` de cada dependencia de la biblioteca, por nombre y SHA-256; una dependencia ausente o distinta en el
  build consumidor provoca el respaldo nativo;
- la revisión de LLVM (`LLVM_REVISION` cuando el host la expone);
- el runtime configurado, por SHA-256;
- la auditoría de opciones de ABI: nombre y versión del pipeline y modo de punto flotante, FMA, redondeo y
  denormales, además del target, el data layout, la optimización y la depuración que ya formaban parte de ella.

Un sidecar v1 se rechaza por su versión y cae al respaldo nativo. Los sidecars son entradas del manifiesto del
bundle consumidor, así que alterarlos invalida el bundle en vez de restaurar un ejecutable viejo.

`nsc restore` copia el sidecar junto con el paquete estático y lo registra en el lock y en la procedencia. Nunca se
publica ni se consume entre DLL: una referencia a DLL enlaza siempre su biblioteca de importación.

Fuera de alcance: referencias a DLL (por diseño) y `full`.

### 11.4 Pruebas unitarias

| Prueba | Qué demuestra |
|---|---|
| `Item391ThinLtoTests` | Backends reales sin aviso en `-O2`, índice reproducible, segundo build con caché que restaura cada backend, ejecutable y PDB idénticos con `-O2 -g`, aviso en `-O0`, sidecar publicado, consumo con imports, respaldo nativo con motivo, consumo `-g` reproducible, ejecutable thin no mayor que el particionado y cadena de bibliotecas con dependencia alterada y sidecar v1. |
| `ProjectOopLoweringTests` (`thinlto-cross-partition-inlining`) | En el IR final de `Main` no queda ninguna llamada a funciones definidas en otra partición. |
| `ProjectOopLoweringTests` (`thinlto-internalization`) | Una función huérfana desaparece sólo cuando se preserva únicamente `Main`. |
| `Item392IncrementalLinkTests` | Editar un archivo con `-g` y `-gline-tables-only` vuelve a bajar menos unidades que el total; la línea de un pánico sigue a la edición. |
| `ProjectCommandsTests` (restauración thin) | El sidecar de un paquete estático se restaura y entra al lock; el de una DLL no. |

El usuario ejecutó las suites completas en Debug y Release, con todas sus variantes, sin fallos.

### 11.5 Matriz de línea de comandos

Ejecutada sobre `nsc.exe` con 58 comprobaciones, todas correctas:

- `-O2` y `-O3`, cada uno con `-g0` y `-g`, cada uno con `-j1` y `-j16`, en caché fría y caliente: la salida del
  programa es la del build particionado, el ejecutable y el PDB calientes son idénticos a los fríos y `-j1` escribe
  el mismo ejecutable y el mismo PDB que `-j16`;
- biblioteca estática por referencia directa: sidecar consumido, respaldo nativo con motivo y build `-g`
  reproducible;
- biblioteca restaurada por `nsc restore`: lock con sidecar y sidecar consumido;
- cadena de bibliotecas: dos sidecars consumidos y respaldo cuando la `.nsmeta` de una dependencia difiere;
- DLL con ThinLTO: conserva sus tres exports y su consumidor funciona.

### 11.6 Pendiente

Nada dentro del alcance de LTO. Las cifras de beneficio y costo por configuración (LTO-05) pertenecen a
`NSC-MEDICION-PARTE-2.md`.
