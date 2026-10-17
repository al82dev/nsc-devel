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
| Naturaleza | Instrumentación diagnóstica, optativa y sin efecto semántico. |
| Repositorio | Sólo `A:\nsc-devel`. |
| Componentes | Driver, `ProjectCompilation`, `ProjectGenericModel` y prueba focal del driver. |
| Comportamiento preservado | Sintaxis, diagnósticos, especializaciones, orden, artefactos, ABI y NSMETA. |
| Consola | El progreso normal conserva su salida concisa; el detalle no se imprime en consola. |
| Archivo | Sólo se escribe cuando el usuario proporciona `--generics-trace=<file>`. |
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

## Contrato de `--generics-trace`

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
8. no se crea cuando la opción no está presente.

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

El escenario externo debe repetirse con el `nsc.exe` construido por este commit y añadir una ruta
explícita al comando, por ejemplo:

```text
--generics-trace=<ruta-escribible>\nsc-generics-trace.log
```

Después del `0xC00000FD` se conserva el archivo completo. No se modifica código causal hasta leer:

1. el build hash del encabezado;
2. la última subfase iniciada;
3. la última unidad o demanda iniciada sin su cierre;
4. la secuencia de especializaciones abiertas que la precede.

L05 y todo trabajo posterior permanecen detenidos.
