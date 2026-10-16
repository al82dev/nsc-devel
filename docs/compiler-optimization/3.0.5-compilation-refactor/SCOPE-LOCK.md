# SCOPE LOCK — NativeSharp Compiler 3.0.5: rendimiento y refactor del proceso de compilación

## Estado del documento

Este candado fija el alcance de la auditoría y del diseño solicitados para NativeSharp Compiler
3.0.5. No autoriza por sí mismo ninguna implementación. Cualquier tanda posterior deberá declarar
su propio SCOPE LOCK, demostrar equivalencia observable y cerrarse con pruebas focales antes de
avanzar a la siguiente.

## Alcance autorizado

| Dimensión | Decisión vinculante |
|---|---|
| Rama de trabajo | `development`. |
| Punto de rescate | La rama `v.3.0.4-baseline` y su baseline constituyen la referencia congelada. No se modifican desde este trabajo. |
| Producto | Exclusivamente `nsc`, su proceso de compilación y la infraestructura interna directamente necesaria para medirlo, modularizarlo y optimizarlo. |
| Neutralidad de la carga | El problema pertenece al pipeline general de `nsc`. |
| Corpus primario | La medición comienza con un programa mínimo controlado —incluido `Hola mundo`— y continúa con fixtures sintéticos de complejidad creciente, todos propiedad de NSC. |
| Trabajo de este expediente | Auditoría estática exhaustiva, diseño formal, inventario método por método, estrategia de medición, caché, incrementalidad, paralelismo, eliminación de recursión y plan de refactor. |
| Contratos congelados | Sintaxis, semántica observable, diagnósticos, ABI, layout, ownership, nombres físicos, orden determinista, formatos públicos, CLI existente y comportamiento de O0/O1/O2/O3. |
| NSMETA | No se reintroduce el versionado de NSMETA como feature independiente. El diseño puede consumir la metadata actual y puede definir cachés internas privadas, invalidadas por esquema, sin convertirlas en contrato público ni alterar por esa vía el formato congelado. |
| Baseline | Lo ejecuta el usuario desde la GUI en otra máquina. Este expediente no lo ejecuta, no lo cancela y no interpreta un resultado todavía no entregado. |
| Código | No se modifica código de producción ni pruebas en esta fase. |
| Unidades y herramientas | Sólo archivos y artefactos bajo `A:`. Herramientas invocadas por nombre desde el PATH de Windows. Sin MSYS, sin workspaces auxiliares de Codex, sin escrituras en `C:`. |
| Red | Sin `push`, `pull` ni `fetch`. |

## Preguntas que sí debe contestar el diseño

1. Qué trabajo ejecuta cada fase de compilación y qué métodos concentran el costo o el riesgo.
2. Por qué una compilación observada dedicó aproximadamente 122 s a `generics`, 53 s a LLVM y
   9 s al enlace, y qué parte es hecho confirmado, inferencia sustentada o hipótesis pendiente de
   instrumentación.
3. Qué costo fijo paga un programa mínimo y por qué fases que no necesita trabajo sustancial siguen
   ejecutándose o reconstruyéndose.
4. Por qué `artifact-cache-hits: 0` puede ocurrir y qué sí/no cubre la caché actual.
5. Qué estructuras producen recorridos repetidos, búsquedas lineales, clonación redundante,
   reparsing de plantillas o reconstrucción de estado.
6. Cómo convertir las rutas recursivas del pipeline caliente en algoritmos iterativos con pilas o
   colas explícitas, sin cambiar el orden ni los diagnósticos.
7. Cómo usar varios procesadores con resultados deterministas, propiedad clara y sin carreras.
8. Cómo dividir archivos de 27 k, 20 k, 16 k y 10 k líneas en componentes cohesionados sin mover
   contratos públicos ni hacer un “big bang”.
9. Qué tandas, pruebas focales, telemetría y criterios de rechazo deben gobernar la implementación.

## Fuera de alcance

- Agregar sintaxis, palabras reservadas, tipos, reglas semánticas o características del lenguaje.
- “Optimizar” eliminando validaciones, diagnósticos, seguridad de ownership o casos límite.
- Cambiar el ABI, la representación pública o los nombres físicos para ganar tiempo.
- Hacer una reescritura total del compilador.
- Introducir un daemon como requisito para obtener corrección; un modo servidor podrá evaluarse
  sólo después de que la compilación normal sea incremental, determinista y cacheable.
- Publicar cachés internas como artefactos portables o como una nueva generación de NSMETA.
- Ejecutar la batería completa durante cada tanda. La metodología seguirá siendo: cambio aislado,
  pruebas O0 focales, expediente, tracking y commit local; la GUI ejecutará el baseline al final.
- Modificar cualquier producto fuera de `nsc` para compensar, medir u ocultar costos del compilador.

## Reglas de verdad documental

Cada conclusión usa una de estas etiquetas:

- **HECHO:** observado directamente en el árbol actual, en su historial o en una medición entregada.
- **INFERENCIA:** explicación compatible con el código y los datos, todavía no aislada por contador.
- **PROPUESTA:** diseño futuro; no existe todavía y no debe presentarse como implementado.
- **GATE:** condición obligatoria para aceptar una tanda.

## Condición de terminación de esta fase

La fase documental termina cuando exista un índice maestro y capítulos enlazados que cubran:

- pipeline y costos;
- inventario método por método de las rutas calientes;
- arquitectura objetivo de caché e incrementalidad;
- algoritmos iterativos y paralelismo determinista;
- descomposición concreta de los monolitos;
- secuencia de implementación con pruebas, métricas, rollback y definición de terminado.

La existencia de esos documentos no equivale a rendimiento demostrado. Los objetivos de tiempo
quedarán como presupuestos provisionales hasta que la instrumentación de la primera tanda produzca
mediciones reproducibles sobre el corpus controlado de NSC. La primera referencia será un programa
mínimo controlado.
