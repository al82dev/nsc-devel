# NativeSharp Compiler 3.0.5 — diseño formal de rendimiento y refactor del proceso de compilación

## Propósito

Este expediente define cómo convertir la línea `development` de `nsc` en un compilador más rápido,
incremental, paralelizable, determinista y mantenible, usando 3.0.4 como referencia de conducta
congelada. No agrega lenguaje y no cambia contratos. El objetivo es reducir trabajo, no hacer menos
comprobaciones.

El diseño nace de dos fuentes complementarias:

- una medición real aportada durante la compilación de un juego sobre SaruDX: aproximadamente
  **122 s en `generics`, 53 s en LLVM, 9 s en enlace y 0 hits de caché de artefactos**;
- una auditoría estática del árbol actual de `development`, incluidos los coordinadores, los modelos
  del frontend, el parser, HIR, MIR, el lowerer LLVM, la partición, el generador de objetos, la caché
  y el pool de trabajadores.

## Conclusión ejecutiva

**HECHO.** El cuello dominante de la medición es `generics`: 122 de 184 segundos observados, cerca
de 66.3 %. LLVM representa 28.8 % y el enlace 4.9 %. Optimizar sólo el linker no puede resolver el
problema principal.

**HECHO.** La caché persistente actual se consulta dentro de `ProjectOopObjectGenerator`, después de
haber terminado sintaxis, genéricos, nombres, miembros, cuerpos, construcción, layouts, HIR y MIR.
Puede evitar bajar/emitar determinados objetos, pero no evita ninguno de los 122 segundos de la
fase `generics`.

**HECHO.** `ProjectCompilation::compileProjectGenerics` crea un `ProjectGenericModel` nuevo en cada
invocación, junta todas las especializaciones importadas y ejecuta `ProjectGenericModel::build` de
forma serial. El método no recibe `compilationJobs_`.

**HECHO.** `compileSyntaxCore` vuelve a insertar como unidades sintácticas la API pública y el
texto fuente de plantillas de cada módulo precompilado. Las plantillas se marcan deliberadamente
como no precompiladas para que el consumidor las monomorfice. Después todas esas unidades pasan por
lexing y parsing. La decisión conserva semántica, pero repite trabajo en cada proceso.

**HECHO.** La caché de especializaciones de `ProjectGenericModel` es sólo una tabla en memoria para
la vida de una compilación. No existe una caché persistente de catálogo genérico, AST decodificado,
demandas, especializaciones semánticas, HIR o MIR.

**HECHO.** La fase genérica combina recorridos completos del AST, copias de scopes, clonación de
subárboles y búsquedas lineales sobre `templateStates_`. Varias rutas consultan el catálogo tres o
más veces para una sola llamada genérica.

**INFERENCIA FUERTE.** En un consumidor pequeño que referencia una superficie grande de SaruDX y
bindings, el costo proviene de la suma de: rehidratar y parsear plantillas importadas, reconstruir
el catálogo completo, recorrerlo repetidamente, descubrir dependencias en todo el AST y clonar las
especializaciones alcanzadas. La evidencia estática explica la forma del costo, pero todavía no
identifica qué plantilla o método consume cada milisegundo.

**ACLARACIÓN.** `artifact-cache-hits: 0` no demuestra por sí solo que “todo SaruDX se recompiló”.
Puede significar caché deshabilitada, claves distintas, configuración no elegible, primera ejecución
o misses reales. La telemetría actual no enumera razón de miss ni módulo. La primera tanda debe
convertir esa ambigüedad en datos.

## Arquitectura recomendada

La solución no es una única optimización. Son cinco fronteras coordinadas:

1. **Medición causal.** Instrumentar el interior de `generics` y cada unidad LLVM: cantidad de
   plantillas, consultas, candidatos, nodos visitados, clones, bytes, demandas, especializaciones,
   hits, misses y razón de invalidación.
2. **Índices algorítmicos.** Sustituir recorridos de `templateStates_` por índices inmutables de
   clave, sintaxis, módulo, dueño, nombre, aridad y cuenta de parámetros. Compartir el resultado de
   una resolución dentro de la llamada, sin volver a calcular la misma escala.
3. **Worklist iterativa.** Separar descubrimiento de demandas y materialización. Usar estados
   `Unseen`, `Queued`, `Building`, `Ready` y `Failed`; pilas explícitas para AST/tipos; cero
   recursión directa en la ruta caliente.
4. **Caché persistente por capas.** Reusar primero el catálogo/AST de plantillas importadas, luego
   especializaciones semánticas y MIR, y finalmente los objetos que ya cubre la caché actual. Cada
   nivel tiene una clave y un contrato de invalidación independiente.
5. **Paralelismo determinista.** Ejecutar demandas independientes y callables en workers locales,
   publicar en slots estables y fusionar por orden canónico. Un único presupuesto `-j`, sin pools
   anidados ni mutación concurrente del AST compartido.

## Resultado esperado por escenario

Los números siguientes son **presupuestos de aceptación propuestos**, no resultados obtenidos:

| Escenario | Trabajo que debe quedar | Objetivo provisional |
|---|---|---:|
| Cold build limpio | Todo el pipeline, con mejores algoritmos y paralelismo | No empeorar más de 5 % frente al 3.0.4 equivalente; meta inicial: reducir al menos 30 %. |
| Warm build sin cambios | Validación de claves, materialización final si aplica | Menor a 10 % del cold build y, como meta de producto, menor a 15 s en el caso real. |
| Cambio sólo en el juego | Recompilar el juego y el cierre afectado | Ninguna especialización ni objeto de SaruDX/bindings sin cambios debe regenerarse. |
| Cambio privado en una dependencia | Sólo nodos cuya huella de implementación dependa de él | No invalidar consumidores guiados exclusivamente por la interfaz. |
| Cambio público/ABI | Cierre transitivo correcto | Invalidación explícita, explicable y sin reutilización insegura. |
| `-j1` frente a `-jN` | Mismo resultado observable | Mismos diagnósticos ordenados y hashes de artefactos; `-jN` reduce pared sin sobrepasar presupuesto. |

## Índice del expediente

- [SCOPE-LOCK.md](SCOPE-LOCK.md): límites vinculantes y contratos protegidos.
- [01-AUDITORIA-PIPELINE-Y-COSTOS.md](01-AUDITORIA-PIPELINE-Y-COSTOS.md): pipeline actual,
  causas confirmadas, complejidad y mapa de deuda.
- [02-DISENO-INCREMENTAL-Y-PARALELO.md](02-DISENO-INCREMENTAL-Y-PARALELO.md): arquitectura
  objetivo, claves, worklists, cachés, concurrencia, determinismo y eliminación de recursión.
- [03-PLAN-MODULARIZACION-METODO-A-METODO.md](03-PLAN-MODULARIZACION-METODO-A-METODO.md):
  partición concreta de clases y tratamiento de cada familia de métodos grandes.
- [04-TANDAS-GATES-Y-VALIDACION.md](04-TANDAS-GATES-Y-VALIDACION.md): secuencia de entrega,
  pruebas focales, métricas, criterios de rechazo y definición de terminado.
- [APPENDIX-A-METHOD-INVENTORY.md](APPENDIX-A-METHOD-INVENTORY.md): inventario mecánico completo
  de todas las funciones definidas en quince archivos críticos, con líneas, tamaño, ramas, bucles,
  llamadas y recursión directa detectada por libclang 23.1.0.

## Decisiones que no deben perderse durante la implementación

- 3.0.5 es una versión de rendimiento/refactor sobre contratos congelados, no una nueva edición del
  lenguaje.
- La rama salvavidas 3.0.4 no recibe estas modificaciones.
- Una caché no es correcta sólo porque acelera: debe poder explicar la identidad de la entrada, la
  razón de cada miss y la cadena de dependencias que invalidó el artefacto.
- “Alcanzable” significa cierre semántico conservador y demostrable. No significa podar cuerpos por
  intuición ni omitir materialización que pueda observar reflexión, metadata o ABI.
- El paralelismo no puede cambiar el orden de diagnósticos, nombres generados, ordinals, dumps ni
  bytes. Los workers producen resultados aislados; un coordinador los publica.
- Eliminar recursión no significa ocultarla dentro de `std::function`: se requieren pilas de frames
  explícitas y pruebas de profundidad.
- Modularizar no es repartir un método gigantesco en varios `.cpp` mediante regiones. Cada módulo
  nuevo debe poseer una responsabilidad, entradas inmutables, salida explícita y pruebas propias.
- Ninguna tanda se marca terminada por compilar. Debe probar equivalencia focal, documentar la
  medición antes/después y dejar el worktree listo para un commit local aislado.

## Estado de validación de este expediente

Se realizó análisis estático y se generó el inventario AST contra la base de compilación existente.
No se modificó código de producción, no se ejecutaron pruebas, no se ejecutó baseline y no se hizo
ninguna afirmación de mejora medida. La medición causal comienza en la primera tanda de
implementación, después de que el baseline externo confirme el punto de rescate.
