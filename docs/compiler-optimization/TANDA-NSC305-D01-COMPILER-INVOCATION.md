# Tanda NSC 3.0.5 - D01: Compiler Invocation

## Resultado

Se extrajo de `Driver::run` la interpretación y normalización de la invocación del
compilador hacia `Driver/CompilerInvocation`. El cambio es una separación interna:
la interfaz de línea de comandos, sus valores predeterminados, diagnósticos,
acciones, rutas, perfiles, ABI y NSMETA conservan su contrato previo.

**ESTADO: COMMIT INTERMEDIO; D01 NO ESTÁ CERRADO.** Las unidades extraídas cumplen
la cota estructural, pero `Driver::run`, modificado por esta hoja, todavía contiene
730 líneas ejecutables (1,082 físicas, líneas 3951-5032 después de la extracción).
La regla obligatoria de máximo 60 ejecutables exige enlazar inmediatamente D02 y
extraer el despacho restante. Este expediente no autoriza marcar D01 en tracking.

**VALIDACIÓN DIFERIDA A LA TANDA INTEGRAL.**

Por instrucción expresa para esta tanda, no se compiló el producto y no se
ejecutaron pruebas. Este expediente registra únicamente los gates estáticos del
cambio. La validación dinámica corresponde a la tanda integral posterior.

## SCOPE LOCK

### Autorizado

- Extraer de `Driver::run` el reconocimiento de opciones y sus valores.
- Centralizar la normalización de la invocación en un objeto de estado con
  propiedad explícita sobre las cadenas creadas durante la carga del proyecto.
- Extraer la validación de combinaciones opción/acción que precede al despacho.
- Reducir el tamaño y las responsabilidades de `Driver.cpp`.
- Registrar la nueva unidad de compilación en CMake.

### Prohibido

- Agregar, retirar, renombrar o reinterpretar opciones de CLI.
- Cambiar precedencias entre CLI, proyecto, perfil y valores predeterminados.
- Cambiar textos, orden o códigos de salida de diagnósticos.
- Mover o alterar el despacho de acciones, reservado para D02.
- Cambiar rutas, publicación de artefactos, ABI, NSMETA o formatos persistentes.
- Introducir recursión, paralelismo, caché o estado global.
- Editar tracking externo o trabajo concurrente.

## Archivos y símbolos

### Nuevos

- `src/Driver/CompilerInvocation.h`
  - `CompilerInvocation`
  - `parseCompilerInvocation`
  - `validateCompilerInvocation`
  - `validateCompilerLinkOptions`
- `src/Driver/CompilerInvocation.cpp`
  - carga de valores predeterminados del proyecto;
  - resolución de perfiles y modos de punto flotante;
  - validación previa al despacho;
  - derivación de la configuración predeterminada de caché.
- `src/Driver/CompilerOptionParser.cpp`
  - recorrido lineal único y ordenado de `argv`;
  - manejadores libres y acotados por familia de opción;
  - conservación de precedencias, diagnósticos y consumo de argumentos.

### Modificados

- `src/Driver/Driver.cpp`
  - delega la preparación de la invocación;
  - conserva instalación de scopes, reporte, poda de caché y despacho de acciones.
- `CMakeLists.txt`
  - registra `src/Driver/CompilerInvocation.cpp` y
    `src/Driver/CompilerOptionParser.cpp` en `nsc_driver`.

## Contratos preservados

### CLI y diagnóstico

Se conservan las familias de opciones existentes:

- modos de emisión y acciones;
- optimización, perfil y presupuesto O2;
- información de depuración y mapas de prefijos;
- LTO, enlace incremental, subsistema y bibliotecas adicionales;
- modelo de punto flotante;
- trabajos, núcleos reservados, timeout y profundidad/cantidad genérica;
- progreso, tiempos y traza LLVM;
- política, raíz y límite de caché;
- raíz de referencias y opciones provenientes del proyecto.

La extracción no crea aliases, opciones implícitas ni nuevos defaults. Los errores
mantienen los textos y `ExitCode` anteriores.

### Orden y precedencia

La secuencia observable permanece:

1. Interpretación de opciones de consola y comandos de proyecto.
2. Instalación de scopes del driver.
3. Interpretación y normalización de la invocación.
4. Aplicación de defaults del proyecto cuando no hubo opción explícita.
5. Resolución de perfil y defaults de punto flotante.
6. Instalación y reporte del modo de punto flotante.
7. Rechazo de una invocación sin acción.
8. Validación de combinaciones previas al caché y derivación de su default.
9. Poda de caché con sus efectos y reporte históricos.
10. Validación posterior de LTO/enlace incremental y reporte de perfil.
11. Despacho de acciones sin cambios en `Driver::run`.

### Propiedad y vida útil

Los argumentos originales continúan representados mediante `std::string_view`
prestados por el llamador. La ruta de salida que puede originarse al cargar el
proyecto tiene almacenamiento propietario dentro de `CompilerInvocation`; por
ello, cualquier vista agregada a los argumentos normalizados permanece válida
durante todo el despacho.

### Casos sensibles auditados

- `--time` queda aplicado incluso si la preparación termina con diagnóstico.
- El formato de progreso no habilita progreso por sí solo.
- La precedencia de opciones explícitas sobre settings del proyecto se conserva.
- Los defaults del perfil se aplican únicamente cuando no hubo valor explícito.
- La opción de traza LLVM conserva su consumo previo sin adquirir semántica nueva.
- La raíz explícita de caché se instala antes de la primera operación de caché.
- La poda/reporte de caché permanece antes del rechazo de LTO o enlace incremental no aplicable.
- El parser comentado de `--generics-trace` permanece bajo `INSTRUMENTATION CODE` y
  desactivado; su diagnóstico explícito conserva el `string_view` original y la
  apertura automática continúa antes de cargar defaults del proyecto.
- Las acciones permanecen fuera de `CompilerInvocation` y dentro de `Driver::run`.

## Diseño interno

`CompilerInvocation` es un estado por invocación, sin singleton ni estado mutable
compartido. Se pasa por referencia a las tres operaciones de preparación y después
se consume de forma local por `Driver::run`.

`CompilerOptionParser.cpp` contiene un único recorrido de argumentos. Cada posición
se consulta contra una tabla ordenada de manejadores libres; cada manejador reconoce
una familia y termina inmediatamente si no coincide. La tabla preserva el orden
histórico: telemetría, progreso, instrumentación, LTO, FP, perfil, política de jobs,
caché, referencias, límites genéricos, timeout, presupuesto, subsistema, enlace,
depuración y optimización. Los defaults de proyecto/perfil y las validaciones viven
en funciones separadas. No hay recursión, despacho de acciones ni estado global
nuevo.

Ninguna de las 44 funciones nuevas de `CompilerInvocation.cpp` y
`CompilerOptionParser.cpp` excede 60 líneas físicas, una cota más estricta que las
60 líneas ejecutables exigidas. El máximo del parser es de 41 líneas físicas
(`handleLibraryReferenceRoot`); el máximo de defaults/validación es de 38 líneas
físicas (`validateActionOptions`). Esto no cierra D01: `Driver::run` permanece por
encima de la cota y debe resolverse en el enlace obligatorio con D02.

## Integración con Builder 3

Builder 3 configura y construye el grafo declarado por CMake; no mantiene un segundo
inventario de archivos fuente del compilador. Por ello, ambas unidades D01 quedan
visibles al builder al estar registradas en `nsc_driver`. Los catálogos de Builder 3
enumeran casos/variantes de pruebas, no fuentes internas del target. D01 no agrega
casos nuevos y no requiere modificar esos catálogos.

## Gates estáticos

- Revisión diferencial limitada a los cinco archivos funcionales y este
  expediente.
- Verificación de whitespace mediante `git diff --check`.
- Inventario de opciones y diagnósticos contra la implementación anterior.
- Verificación de que el despacho de acciones no aparece en la nueva unidad.
- Verificación de que no existen llamadas recursivas ni estado global nuevo.
- Verificación de longitudes de línea conforme a los lineamientos.
- Verificación del contenido exacto del staging antes del commit.

Resultados del cierre estático:

- inventario de opciones anterior/nuevo: sin diferencias, incluida la instrumentación desactivada;
- 44 funciones nuevas extraídas: cero funciones mayores de 60 líneas físicas o
  ejecutables;
- `Driver::run`: 1,082 líneas físicas y 730 ejecutables; gate estructural pendiente;
- `parseCompilerOptions`, `parseCompilerInvocation`, `validateCompilerInvocation` y
  `validateCompilerLinkOptions`: cero autorrecursiones;
- despacho en `CompilerInvocation.cpp` y `CompilerOptionParser.cpp`: cero llamadas `run*`;
- integración fuente: ambas unidades D01 aparecen exactamente una vez en `nsc_driver`;
- líneas nuevas mayores de 120 caracteres: cero;
- `git diff --check`: limpio;
- staging limitado a los seis archivos D01 antes del commit.

## Validación dinámica diferida

La tanda integral deberá cubrir, como mínimo:

- construcción del target oficial de NSC;
- pruebas focales de parsing y validación de CLI;
- invocaciones positivas y negativas representativas por familia de opciones;
- proyectos con y sin defaults configurados;
- combinaciones de perfil, depuración, LTO, caché y punto flotante;
- suite externa focal del driver y, después, el gate integral autorizado.

No se reclama compilación correcta ni paridad dinámica antes de esa tanda.

## Enlace obligatorio siguiente

D02 debe comenzar inmediatamente sobre este commit intermedio y extraer el despacho
de acciones hasta dejar `Driver::run` y todos sus helpers en un máximo de 60 líneas
ejecutables. D01 sólo podrá cerrarse junto con esa evidencia estructural. Esta hoja
no modifica emisión de fuente pública ni publicación de artefactos, que corresponden
respectivamente a D03 y D04.
