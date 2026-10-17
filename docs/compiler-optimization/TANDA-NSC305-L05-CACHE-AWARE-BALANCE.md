# Tanda NSC305-L05 — Particiones balanceadas y conscientes de caché

> Campaña NativeSharp Compiler 3.0.5. Producto único: `nsc`.
> Rama de implementación: `development`.

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Combinar balance determinista de las particiones LLVM con reutilización precisa de objetos. |
| Naturaleza | Optimización y refactor interno no funcional. |
| Referencia congelada | `v.3.0.4-baseline`; inicio de la tanda en `4f5750930`. |
| Comportamiento idéntico | Sintaxis, semántica, diagnósticos, API, ABI, layout, ownership, NSMETA, MIR, LLVM observable y artefactos finales. |
| Repositorio | Sólo `A:\nsc-devel`. |
| Componentes | Particionador OOP LLVM, caché privada de artefactos y consumidor inmediato en el generador de objetos. |
| Archivos de producción permitidos | `include/nativesharp/LLVM/CompilationArtifactCache.h`, `src/LLVM/CompilationArtifactCache.cpp`, `include/nativesharp/LLVM/ProjectOopCodePartition.h`, `src/LLVM/ProjectOopCodePartition.cpp`, `src/LLVM/ProjectOopObjectGenerator.cpp`. |
| Prueba permitida | Extender exclusivamente la prueba interna de partición/incrementalidad en `tests/unit/ProjectOopLoweringTests.cpp`. |
| Tracking permitido | Este expediente y `A:\handover-deepseek\restantes-optimizacion-deepseek.md` al cerrar la tanda. |
| Protegido | Lowerer LLVM, modelos frontend/HIR/MIR, parser, driver, runtime, bibliotecas, proyectos, fixtures de lenguaje, manifiestos, NSMETA y todo producto distinto de `nsc`. |
| Prohibido | Baseline, batería completa, deploy, instalación, red Git, cambios de contratos y cualquier versionamiento NSMETA independiente. |
| Validación | Target focal, prueba de partición/incrementalidad O0, equivalencia de asignación y hits/misses, `git diff --check`. |
| Cierre | Un cambio de cuerpo conserva el plan y produce un solo miss; el build repetido restaura todas las unidades; el plan frío está balanceado; expediente, tracking y commit local aislado. |

## 2. Cadena causal

1. Con caché habilitada, el generador selecciona actualmente `StableIncremental`.
2. Esa política calcula `hash(nombre) % unidades`, lo que conserva buckets al cambiar un cuerpo,
   pero no considera el costo de las funciones y puede concentrar trabajo pesado.
3. La política `Balanced` usa LPT y distribuye el costo, pero recalcularla después de un cambio de
   cuerpo puede mover funciones no modificadas e invalidar objetos que todavía eran reutilizables.
4. `historicalCost` se actualiza dentro de un particionador de uso único y se pierde al terminar la
   llamada; actualmente no constituye historia entre compilaciones.
5. L05 necesita separar dos decisiones: el primer plan se balancea, y las compilaciones posteriores
   reutilizan exactamente ese plan mientras su topología siga siendo compatible.

## 3. Diseño autorizado

- La clave privada del plan incluye versión de esquema, número solicitado de unidades y la secuencia
  canónica de identidades físicas de funciones definidas. No incluye cuerpos.
- Un miss del plan crea una distribución LPT determinista con el costo MIR estático existente.
- Un hit restaura la misma asignación función-unidad y el peso observado de cada unidad.
- El payload es privado, verificable y publicado atómicamente bajo la raíz de la caché de artefactos.
  No modifica ni se publica como NSMETA.
- El plan restaurado se valida completamente: esquema, cardinalidades, ordinals, cobertura única y
  ausencia de buckets vacíos. Un payload incompatible se trata como miss y se reemplaza sólo tras
  una generación correcta.
- Las claves de objetos continúan derivándose del MIR real de definiciones y declaraciones. El plan
  nunca convierte una clave distinta en hit ni debilita la verificación COFF.
- El costo observado sólo actualiza el perfil después de una generación exitosa. La asignación no se
  modifica por ese dato durante una compilación warm, para no propagar invalidaciones.

## 4. Invariantes

- Cada función definida pertenece exactamente a una unidad; imports pertenecen a ninguna.
- `stableKey`, ordinals, nombres físicos, declaraciones y orden canónico no dependen del scheduling.
- El mismo conjunto de funciones y el mismo `-j` restauran el mismo plan aunque cambie un cuerpo.
- Un cambio de topología o de cantidad de unidades usa otra clave y genera un nuevo plan balanceado.
- La corrupción o publicación parcial jamás produce un hit.
- Caché deshabilitada conserva la política `Balanced`; ThinLTO sin caché conserva su política estable
  anterior.

## 5. Rechazo inmediato

La tanda se rechaza si cambia más de una partición ante el fixture de un único cuerpo modificado,
si una partición con la misma clave MIR se reconstruye, si se acepta un payload inválido, si cambia
la salida observable, o si la solución requiere tocar cualquiera de los componentes protegidos.

## 6. Implementación cerrada

- `CompilationArtifactCache` publica el plan privado bajo `partitions/<prefijo>/` con payload,
  metadata `NSCPART 1`, tamaño y SHA-256. La publicación usa el mismo lock por entrada y reemplazo
  atómico que la caché de objetos. Un par ausente o inválido es miss; un par corrupto se pone en
  cuarentena. El pruner contabiliza y expulsa planes y objetos dentro del mismo presupuesto.
- `ProjectOopCodePartition::persistentPlanKey` deriva la identidad topológica a partir del esquema,
  la cantidad efectiva de unidades y las identidades físicas canónicas. Los cuerpos quedan fuera de
  esa clave deliberadamente.
- `CacheAwareBalanced` crea por LPT el plan frío y, en un hit válido, repone exactamente la
  asignación función-unidad. También conserva el costo observado acotado de cada unidad.
- `ProjectOopObjectGenerator` restaura el plan antes de bajar LLVM y lo publica únicamente después
  de que todas las unidades terminan correctamente. Un hit de objeto no reemplaza el costo histórico
  con el costo barato de una lectura de caché.
- Las claves de objeto, la validación COFF, los modelos MIR y el contenido de NSMETA no se cambiaron.
- El binario de prueba conserva `int main()` y su ejecución completa habitual. La variable
  `NSC_PROJECT_OOP_LOWERING_FOCUS=precise-incremental-invalidation` sólo permite ejecutar el caso
  focal sin depender de pruebas históricas de enlace ajenas a L05.

## 7. Evidencia focal

Fecha: 2026-10-17. Rama: `development`. Base de la tanda: `4f5750930`.

1. `cmake --build A:\nsc-devel\build-test-infrastructure --target
   nsc_project_oop_lowering_tests --config Release --parallel 16`: PASS.
2. `A:\nsc-devel\build-test-infrastructure\tests\nsc_project_oop_lowering_tests.exe`, con
   `NSC_PROJECT_OOP_LOWERING_FOCUS=precise-incremental-invalidation` y `TEMP`/`TMP` en `A:\temp`:
   `PASS: 13 checks`, exit 0.
3. El caso focal prueba explícitamente:
   - plan frío idéntico al balance LPT determinista;
   - clave topológica idéntica entre revisiones que sólo cambian un cuerpo;
   - fallback frío idéntico ante payload semánticamente inválido;
   - publicación y restauración íntegra del plan privado;
   - conservación exacta de todos los buckets tras modificar un solo cuerpo;
   - primer build con cero hits;
   - revisión modificada con exactamente un miss;
   - repetición sin cambios con cero misses y todos los objetos restaurados.
   - expulsión del plan privado junto con objetos al aplicar un presupuesto de cero bytes.
4. `git diff --check`: PASS.

Como control adicional se ejecutó el binario sin selector antes de la aserción final de clave. Las
comprobaciones de L05 presentes en esa corrida pasaron,
pero el proceso completo terminó con 95/97 porque dos comprobaciones históricas de enlace no
encuentran `nslink.exe` en el `PATH` nativo de esta máquina. No se buscó por rutas, no se modificó
`PATH` y no se atribuye ese bloqueo ambiental a L05.

## 8. Gate cuantitativo j1/j16 y RSS

El plan maestro exigía que jN mejorara pared y no excediera el presupuesto de RSS, pero no fijó un
valor numérico. Para que el cierre no dependa de una interpretación posterior, esta tanda adopta un
límite local conservador: en el corpus controlado, la mediana RSS j16 debe ser como máximo 1,5 veces
la mediana j1 y permanecer por debajo de 512 MiB.

Se midió `tests/cc1/c23_views_and_lifetimes`, un proyecto autocontenido y rastreado de NSC con 53
fuentes propias, en O0/g0. Fueron cinco muestras frías alternadas por configuración, cada una con
caché y temporales nuevos. j1 produjo un módulo; j16 produjo 12 módulos (coordinador y 11 unidades).

| Configuración | Mediana LLVM | p95 LLVM | Mediana RSS | Máximo RSS |
|---|---:|---:|---:|---:|
| j1 | 1.744.010 us | 1.779.185 us | 141.156.352 B (134,6 MiB) | 141.217.792 B |
| j16 | 1.670.472 us | 1.714.232 us | 175.468.544 B (167,3 MiB) | 175.804.416 B |

Resultado: j16 reduce la mediana LLVM 4,2 % y el p95 3,7 %. La mediana RSS aumenta 24,3 %
(1,243 veces j1), por debajo de ambos límites. Evidencia cruda:
`A:\temp\nsc305-l05-profile\cold-matrix\measurements.csv` y los diez `nsc.log` subordinados.

Una repetición warm j16 sobre la misma caché informó 12 hits y 0 misses; ninguna unidad se
reconstruyó. Esa fase aún consumió 1.476.600 us, evidencia de que fingerprint/lookup conserva un
costo importante que L05 no oculta y que debe alimentar el perfil post-L05.

Las ejecuciones cuantitativas completaron correctamente LLVM y su publicación en caché, pero el
producto EXE se detuvo después porque `nslink.exe` no está en el `PATH`. El gate L05 evalúa la fase
LLVM ya terminada; no se reclama enlace ni ejecución del artefacto a partir de estas muestras.

## 9. Resultado

L05 queda implementado y validado en su alcance focal. Un cambio de cuerpo mantiene la distribución
fría balanceada y sólo reconstruye la unidad cuyo MIR cambió; la siguiente compilación reutiliza la
totalidad de los objetos. j16 mejora de forma medible la pared LLVM fría dentro del presupuesto de
memoria fijado. No se ejecutó baseline, batería completa, deploy ni instalación.
