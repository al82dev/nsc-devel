# Correccion de regresiones O0 posteriores a method bodies

## Bloqueo de alcance

- Se corrige hacia adelante desde `development`; no se restaura un milestone ni se
  retiran las mejoras de rendimiento ya implementadas.
- Ninguna prueba, fixture, proyecto historico, expectativa ni cadena esperada se
  modifica para conseguir un resultado verde.
- No se modifica ni se instala nada en `C:`.
- La validacion de esta serie usa exclusivamente O0. No se ejecutan baseline,
  minibaselines ni matrices O1/O2/O3.
- Cada correccion logica lleva un commit pequeno y un Markdown propio.
- `throws` no forma parte de esta serie: el reporte no contiene `NS3071`,
  `catch it or declare` ni otro diagnostico de propagacion de excepciones.

## Evidencia del reporte externo

El reporte `20261006-181751-ba485e23` contiene 234 fallos, pero no 234 causas
independientes. El analisis estatico los agrupa asi:

| Grupo | Evidencia | Causa primaria |
|---|---|---|
| Edicion del lenguaje | 361 apariciones de `NS3001` y `mixed editions are not allowed` | Los proyectos sin `<Language>` se materializan inmediatamente como Language 2 y pierden el dato de que la edicion fue omitida; despues chocan con Core/NSMETA Language 3 |
| Sintaxis de valores | 34 apariciones de `NS3002` | Efecto secundario: un consumidor sin edicion queda fijado artificialmente a Language 2 antes de analizar su grafo |
| Superficie de Core | `ReferenceCounts`, `ReferenceCountHook`, relaciones no propietarias y overloads de `Console` ausentes | Contratos publicos que deben auditarse despues de retirar la barrera de edicion |
| Compatibilidad de metadata | discrepancias en Stage 48.2, 50.9, 63, 66 e Item 407 | Politicas de compatibilidad que deben validarse por capacidad real, no por etiqueta textual |
| Self-host derivado | cuatro ausencias de SH41/nsc2 | Fallos derivados: SH41 no produce artefactos cuando falla antes la carga/compilacion |

## Causa raiz confirmada de la primera barrera

`ProjectLoader` conserva correctamente Language 2 como valor predeterminado,
pero `Project` almacena solamente el valor efectivo. Al perderse la procedencia,
el compilador no puede distinguir entre:

1. un proyecto que declaro expresamente `<Language>2</Language>`, y
2. un proyecto historico que no declaro ninguna edicion.

`ProjectGraphLoader` y `ProjectGraph::addPrecompiledProjects` comparan despues
los valores efectivos y rechazan ambos casos por igual. Eso convierte una
omision compatible en un conflicto explicito y detiene el analisis antes del
parser, la semantica o el backend.

La correccion pertenece al compilador, no a los proyectos historicos. `Project`
debe conservar si la edicion fue declarada. Un componente conectado que no la
declaro adopta la unica edicion explicita exigida por sus dependencias fuente o
precompiladas. Dos ediciones declaradas y contradictorias siguen produciendo
`NS3001`.

## Invariantes de aceptacion

- Un proyecto aislado sin `<Language>` continua usando Language 2.
- Un grafo sin ediciones explicitas continua usando Language 2.
- Un consumidor implicito de un unico contrato Language 3 adopta Language 3.
- La inferencia se aplica al grafo completo, no solo al borde que fallo primero.
- Language 2 explicito frente a Language 3 explicito sigue rechazado con
  `NS3001`.
- Un propietario implicito puede adoptar la edicion unica publicada por NSMETA.
- Un propietario explicito incompatible con NSMETA sigue rechazado con
  `NS3001`.
- Toda reconstruccion de `Project` conserva edicion, procedencia, paquete,
  amigos y politica de panico.
- Ningun archivo existente bajo `tests/`, ningun fixture y ningun `.nsproj` se
  modifica.

## Tandas correctivas

| Tanda | Alcance | Cierre requerido |
|---:|---|---|
| 1 | Diagnostico y bloqueo de alcance | Causa raiz documentada sin proponer cambios a pruebas |
| 2 | Procedencia e inferencia de edicion en grafos fuente | Compilacion limpia y bateria O0 completa sin fallos |
| 3 | Inferencia de edicion para cierres NSMETA | Compilacion limpia y bateria O0 completa sin fallos |
| 4 | Superficie publica de Core | Compilacion limpia y bateria O0 completa sin fallos |
| 5 | Compatibilidad de metadata basada en capacidades | Compilacion limpia y bateria O0 completa sin fallos |
| 6 | Fallos semanticos/backend restantes | Compilacion limpia y bateria O0 completa sin fallos |
| 7 | Cierre self-host derivado | Compilacion limpia y bateria O0 completa sin fallos |
| 8 | Auditoria final y documentacion | Cero fallos O0; baseline reservado para ejecucion externa |

## Estado

Tanda 1 terminada por analisis estatico. No se ha modificado codigo productivo y
no se ha ejecutado ninguna prueba ni baseline en esta tanda.
