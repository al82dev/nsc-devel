# Tanda correctiva NSC 3.0.5: rastro persistente del fallo en `generics`

## Estado y relación con la tanda

Este documento es un anexo de diagnóstico de la única tanda correctiva abierta para el
`0xC00000FD`. No inicia L05, no abre otra optimización y no presenta la instrumentación como una
corrección del desbordamiento.

El usuario reconstruyó hasta los últimos cambios y repitió el escenario externo. El proceso volvió
a terminar inmediatamente después de `-- NSC: generics` con el código `3221225725`. Por tanto, las
dos correcciones locales anteriores de G09 no cierran el incidente externo.

## SCOPE LOCK de la instrumentación

| Dimensión | Alcance autorizado |
|---|---|
| Objetivo | Persistir el último punto causal alcanzado dentro de `generics` antes de que Windows termine el proceso. |
| Naturaleza | Instrumentación diagnóstica, temporal y sin efecto semántico. |
| Repositorio | Sólo `A:\nsc-devel`. |
| Componentes | Driver, `ProjectCompilation`, `ProjectGenericModel` y prueba focal del driver. |
| Comportamiento preservado | Sintaxis, diagnósticos, especializaciones, orden, artefactos, ABI y NSMETA. |
| Consola | El progreso normal conserva su salida concisa; el detalle no se imprime en consola. |
| Archivo | Temporalmente usa `A:\temp\nsc-generics-trace.log`; una opción explícita reemplaza la ruta. |
| Acciones prohibidas | Aumentar stack, reducir límites, ocultar recursión, corregir causas no demostradas, activar el paralelismo G09, abrir L05, ejecutar baseline, instalar o desplegar. |
| Criterio inmediato | Un build focal produce un log ordenado, vaciado por línea, con build hash y checkpoints internos, sin añadirlos a stdout. |
| Criterio causal posterior | El escenario externo produce un último checkpoint que reduce el defecto a una subfase, unidad, onda, demanda o especialización concreta. |

## Problema de observabilidad demostrado

`GenericTelemetry` medía subfases, pero su resumen sólo podía observarse después de que `build`
regresara. Un `STATUS_STACK_OVERFLOW` termina el proceso dentro de la fase y evita precisamente ese
resumen. La consola únicamente conservaba `-- NSC: generics`, de modo que no distinguía entre:

- expansión de aliases;
- unión de traits;
- índices de retornos y templates;
- descubrimiento de dependencias;
- descubrimiento de demandas raíz;
- demandas de tipos de cuerpos;
- reserva, clonación o aplicación de una onda;
- especialización síncrona de tipo o método.

La ausencia de esa frontera obligaba a tratar muchas funciones recursivas como sospechosas sin una
cadena causal. Eso no satisface la regla de localidad causal.

## Contrato temporal del rastro

Mientras permanezca abierta esta investigación, toda acción sobre un proyecto `.nsproj` abre y
trunca automáticamente:

```text
A:\temp\nsc-generics-trace.log
```

No se requiere modificar la GUI ni su lista de parámetros. Esta activación predeterminada es
instrumentación temporal y debe retirarse después de corregir y verificar el `0xC00000FD`.

La opción explícita conserva prioridad y permite reemplazar la ruta predeterminada:

La nueva opción es:

```text
--generics-trace=<file>
```

También se acepta la forma de dos argumentos:

```text
--generics-trace <file>
```

El archivo:

1. se abre explícitamente y se trunca antes de compilar;
2. comienza con `nsc-generics-trace version=1 build=<hash>`;
3. asigna una secuencia monotónica a cada evento;
4. serializa las escrituras de workers concurrentes;
5. ejecuta `flush()` después de cada línea;
6. no depende de que el proceso termine correctamente;
7. no añade el detalle a stdout ni a stderr;
8. se crea automáticamente para acciones sobre `.nsproj`, aunque la opción no esté presente.

Un trabajo genera siempre un par `WorkStarted` / `WorkFinished` durante una salida normal. Si el
proceso muere dentro del trabajo, el inicio ya quedó persistido y el cierre falta. La última apertura
sin cierre es el breadcrumb causal que debe auditarse.

## Cobertura de checkpoints

Las subfases emiten inicio y fin:

- `generics.visibility`;
- `generics.alias-expansion`;
- `generics.trait-merge`;
- `generics.opaque-results`;
- `generics.ordinary-return-index`;
- `generics.associated-items`;
- `generics.template-collection`;
- `generics.template-index`;
- `generics.dependency-discovery`;
- `generics.imported-specialization-seed`;
- `generics.root-demand-discovery`;
- `generics.body-type-demands`;
- `generics.type-demand-waves`.

Las fronteras de trabajo añaden contexto:

- unidad procesada: módulo, proyecto, fuente y marca de precompilado;
- onda de demandas de cuerpo: ordinal y cardinalidad;
- demanda de cuerpo: sitio estable y tipo;
- onda de tipos: ordinal y cardinalidad;
- reserva de tipo: template canónico y sitio estable;
- clonación worker: clave cerrada, sitio e índice;
- aplicación/commit: clave cerrada y sitio;
- especialización síncrona de tipo o método: template canónico y aridad.

## Hallazgo que refuta la atribución anterior

`ProjectGenericModel::parallelismWorkers_` se inicializa en `1`. La auditoría actual encontró la
definición de `setParallelismWorkers`, pero ninguna llamada de producción desde
`ProjectCompilation` ni desde el driver. `-j 16` configura otras fases, pero no activa la rama
paralela de `executeTypeDemandWaves`.

Consecuencia: las carreras y el fanout corregidos en los commits `6a0ca5a2e` y `ab7cf24c1` eran
defectos reales de la implementación G09 y sus pruebas dirigidas, pero esa rama no explica por sí
sola el desbordamiento observado por el ejecutable de producción. Activar ahora ese cableado sería
una optimización distinta y queda expresamente fuera de este alcance.

## Validación local

- Activación temporal predeterminada, build previo al commit `d8df9949aaafef10`: PASS.
- La ejecución focal de `generics-32` omitió deliberadamente `--generics-trace`: PASS, exit 0.
- `A:\temp\nsc-generics-trace.log` fue creado automáticamente con 231 líneas y 18,403 bytes.
- El encabezado registró `d8df9949aaafef10` y el evento 230 cerró
  `generics.type-demand-waves`; ningún checkpoint detallado apareció en consola.
- Construcción focal de `nsc`: PASS; contrato de runtime estático: PASS.
- Build focal usado por esa validación previa al commit: `649a4b90937db242`.
- Ejecución focal de `generics-32` con `-j 16`, `--progress` y `--generics-trace`: PASS, exit 0.
- El progreso de consola conservó únicamente las fases normales.
- El archivo produjo 231 líneas: encabezado con build hash y 230 eventos ordenados.
- El final contiene la aplicación de las 32 demandas, cierre de la onda y cierre de
  `generics.type-demand-waves`.
- Las aserciones nuevas de `nsc_driver_tests` para creación del archivo y ausencia de detalle en
  stdout no aparecieron entre los fallos. La ejecución completa de ese binario no es verde en esta
  configuración: conserva 22 fallos ajenos preexistentes de Stage 29/30 y linker. No se utiliza esa
  ejecución como prueba de éxito global.

## Siguiente evidencia obligatoria

El escenario externo debe repetirse desde la GUI, sin cambiar sus parámetros, con el `nsc.exe`
construido por este cambio. El directorio `A:\temp` ya fue preparado por el usuario y el compilador
escribirá automáticamente `A:\temp\nsc-generics-trace.log`.

Después del `0xC00000FD` se conserva el archivo completo. No se modifica código causal hasta leer:

1. el build hash del encabezado;
2. la última subfase iniciada;
3. la última unidad o demanda iniciada sin su cierre;
4. la secuencia de especializaciones abiertas que la precede.

L05 y todo trabajo posterior permanecen detenidos.

## Enmienda de alcance tras la primera traza externa

### Evidencia recibida

- Archivo: `A:\temp\nsc-generics-trace.log` de la máquina externa, entregado mediante su carpeta
  compartida.
- Build registrado: `5dd8b3267f4e5336`.
- Tamaño: 31,438 bytes; 309 líneas.
- SHA-256: `8B768108A03AA19BCD59BC5C03EDDF067B1DDED4436E609C1FFE6705C7B607C3`.
- `generics.dependency-discovery` terminó para sus 43 unidades.
- `generics.root-demand-discovery` terminó sus primeras 12 unidades.
- La línea 308 abrió la unidad `NativeSharpDirectX`, `project=7`, `source=0`,
  `precompiled=1`.
- Falta su evento de cierre. El proceso terminó dentro de su llamada a `processNode`.

### SCOPE LOCK de la segunda instrumentación

| Dimensión | Alcance autorizado |
|---|---|
| Entrada exacta | La unidad precompilada `NativeSharpDirectX` durante `root-demand-discovery`. |
| Operación fallida demostrada | `ProjectGenericModel::processNode` no regresa para esa unidad. |
| Invariante violado | Cada unidad iniciada debe producir su cierre antes de avanzar a la siguiente. |
| Causa interna | Todavía no demostrada: puede estar en el nodo, scope, tipo, clonación o inferencia. |
| Cambio autorizado | Breadcrumbs temporales de entrada/salida, sin modificar decisiones semánticas. |
| Símbolos candidatos | Rutas recursivas alcanzables desde `processNode`; lista posterior. |
| Archivos autorizados | `src/Frontend/ProjectGenericModel.cpp`, este expediente y el handover. |
| Archivos protegidos | Parser, metadata, ABI, NSMETA, límites, stack, Builder, GUI y consumidores. |
| Criterio inmediato | La siguiente traza identifica el nodo y la función recursiva abierta al caer. |
| Criterio causal | Una secuencia repetida o anidada demuestra la cadena exacta que debe eliminarse. |

Los símbolos candidatos son `processNode`, `cloneAliasType`, `processTypeSyntax`,
`inferExpressionType`, `inferTypeArguments`, `applyExpectedTypeToExpression`,
`inferGenericCall` y `processGenericCall`.

No se implementará todavía una corrección semántica. La unidad identifica dónde observar, pero no
demuestra por sí sola cuál de las rutas internas es responsable.

### Validación de los breadcrumbs internos

- Build focal previo al commit: `d31382d1c86f78ed`.
- Construcción exclusiva de `nsc`: PASS; contrato estático del runtime host: PASS.
- `generics-32 -j 16 --progress`, sin argumento de traza: PASS, 32/32, exit 0.
- Archivo automático: 2,133 líneas y 279,709 bytes.
- Nodos de `root-demand-discovery`: 526 aperturas y 526 cierres.
- Eventos de `cloneAliasType`: 454, balanceados durante la salida normal.
- Último evento: cierre de `generics.type-demand-waves`.
- El detalle no apareció en la consola.

## Enmienda causal tras la segunda traza externa

### Evidencia recibida

- Archivo local: `A:\temp\nsc-generics-trace.log`.
- Build registrado: `15c0a46663615bda`.
- Tamaño: 25,631,805 bytes; 183,659 líneas.
- SHA-256: `F633706F0230DDD7EE3F81817849A2E2D22270A04F941A588BE8CC075577AC96`.
- La unidad abierta al caer vuelve a ser `NativeSharpDirectX`, `project=7`, `source=0`,
  `precompiled=1`.
- Sus 43,299 eventos `generics.root-demand-node` tienen exactamente 43,299 cierres.
- `cloneAliasType`, `processTypeSyntax`, `inferExpressionType`, `inferGenericCall` y
  `applyExpectedTypeToExpression` también están balanceados.
- El último nodo, un `BlockStatement` con rango `820922..820925`, cerró correctamente.
- No se emitió el cierre de la unidad: el proceso murió después del último nodo y antes de que
  `processNode` regresara.
- La clase `D3d11Constants` aporta 5,813 `FieldDeclaration` y 11,670 nodos descendientes después
  de preparar su alcance.

### Cadena causal demostrada

El commit `c4d9ef53b` convirtió `GenericLookupScope::valueTypes` en una lista persistente enlazada
mediante `std::shared_ptr<const ValueBinding>`. La copia y la inserción pasaron a O(1), pero la
liberación del último propietario quedó delegada a la destrucción recursiva implícita de
`shared_ptr`: destruir un `ValueBinding` destruye su `parent`, que destruye al siguiente, y así
sucesivamente.

`prepareNodeScope` registra todos los campos de una clase en esa cadena. En la unidad externa, el
alcance de `D3d11Constants` conserva 5,813 enlaces y se comparte entre sus descendientes. Después
de cerrar el último nodo desaparece el último propietario de la cola única; su liberación intenta
usar miles de marcos nativos y Windows termina `nsc` con `0xC00000FD`. Esto explica simultáneamente
que todos los trabajos internos estén balanceados y que falte únicamente el cierre de la unidad.

### SCOPE LOCK de la corrección causal

| Dimensión | Alcance autorizado |
|---|---|
| Entrada exacta | Liberación del alcance de `D3d11Constants` después del último nodo de la unidad precompilada. |
| Operación fallida | Destrucción recursiva de la cola única `ValueBinding::parent`. |
| Invariante violado | El tamaño de un alcance léxico no puede consumir profundidad de pila nativa al liberarse. |
| Símbolo responsable | `ProjectGenericModel::GenericLookupScope::ValueBinding`. |
| Archivo funcional | `include/nativesharp/Frontend/ProjectGenericModel.h`. |
| Cambio mínimo | Desacoplar y destruir iterativamente la cola única; detenerse ante el primer ancestro todavía compartido. |
| Comportamiento preservado | Inserción O(1), copia O(1), orden newest-to-oldest, sombreado, tipos encontrados, sintaxis, diagnósticos, ABI y NSMETA. |
| Archivos protegidos | Parser, metadata, ABI, NSMETA, límites, stack, Builder, GUI, consumidores y L05. |
| Pruebas | Build focal de `nsc`, contrato de runtime estático y pruebas O0 focales de genéricos. |
| Acciones prohibidas | Aumentar stack, reducir datos, retirar campos, cambiar resolución, abrir L05, baseline, instalación o despliegue. |
| Criterio de terminación | La liberación profunda se verifica sin recursión, las pruebas focales pasan y el escenario externo deja de producir `0xC00000FD`. |

La regresión focal reutiliza el caso nocturno de escala `TF-FRONT-168`. Su fuente conserva el
fan-out genérico existente y añade una clase con 6,000 campos ordinarios: supera los 5,813 enlaces
observados externamente sin crear 6,000 especializaciones ni modificar un contrato de lenguaje.

### Validación local de la corrección

- Construcción focal de `nsc`: PASS.
- Contrato de runtime estático de `nsc.exe`: PASS.
- Build hash del binario focal: `772c9eea8a11311a`.
- `TF-FRONT-168` Debug, único caso ejecutado: PASS, exit 0, 1,582,469 microsegundos.
- La fuente del caso contiene la clase de 6,000 campos y alcanza `MethodBodies`; por tanto, la
  liberación profunda terminó y el modelo genérico siguió siendo consumible.
- `generics-32`, `--emit-project-oop-obj`, `-O0`, `-j 16`: PASS, exit 0.
- El objeto COFF AMD64 se emitió y verificó; `generics` terminó en 21 ms.
- Un intento separado de producir el ejecutable recorrió correctamente todo el compilador hasta
  `executable-build`, pero no pudo enlazar porque `nslink.exe` no está disponible por nombre en el
  `PATH` de esta máquina. No se usó una ruta absoluta ni un linker sustituto y ese intento no se
  presenta como prueba ejecutable.

La instrumentación automática y los breadcrumbs permanecen temporalmente activos en este commit.
Se retirarán únicamente después de que el mismo escenario externo confirme que ya no termina con
`0xC00000FD`; así, cualquier fallo posterior conservará evidencia causal en vez de volver a dejar
solamente `-- NSC: generics`.
