# Tanda correctiva NSC 3.0.5: G09 y fallo dentro de `generics`

## Alcance bloqueado

Esta tanda corrige exclusivamente la ejecución paralela de las ondas de especialización genérica.
No agrega sintaxis, no modifica ABI ni NSMETA, no inicia L05 y no cambia contratos públicos del
lenguaje. El baseline completo queda fuera de esta tanda.

## Incidente externo

Una compilación O0 con `-j 16` terminó dentro de `generics` con el código Windows `3221225725`
(`0xC00000FD`, `STATUS_STACK_OVERFLOW`). El último progreso publicado fue `-- NSC: generics`;
LLVM y enlace todavía no habían comenzado.

El escenario externo completo no existe en esta máquina. Por ello, el cierre requiere dos niveles
de evidencia: corregir todo defecto demostrable de la ruta concurrente y, después, obtener la
repetición satisfactoria del escenario original por el usuario. Esta tanda no declarará resuelto el
incidente externo sólo porque pasen los fixtures locales.

## Defecto demostrado

El cambio G09 que movió la clonación fuera del mutex dejó cuatro incumplimientos verificables:

1. Cada worker llamaba `cloneNode(state.unit->arena(), ...)`. `AstArena::make` inserta en un
   `std::vector<Entry>` sin sincronización; por tanto, dos clones de la misma onda escribían a la
   misma arena concurrentemente.
2. `SpecializationWorker`, documentado como dueño de una arena privada, nunca se utilizó.
3. El bucle de `std::async` capturaba `reservedDemand` por referencia mediante `[&]`, dejando la
   identidad del elemento dependiente de la variable de iteración.
4. Se creaba una tarea por demanda, sin respetar `parallelismWorkers_` como límite de concurrencia.

También existen escrituras auxiliares de identidad/origen durante `cloneType`. Esas escrituras no
pueden volver a los mapas compartidos desde un worker; deben acumularse en el producto privado y
fusionarse únicamente durante el commit serial.

## Diseño de la corrección

- Cada `SpecializationProduct` posee una `AstArena` privada y mapas privados de identidad/origen.
- El contexto de instanciación usado durante el clone es local al hilo.
- La fase paralela sólo clona y escribe en el producto asignado a su índice.
- El número de tareas es `min(parallelismWorkers_, demandas)`.
- Un contador atómico reparte índices; no se captura la variable del `range-for`.
- Después de esperar a todos los workers, el coordinador recorre los productos en el orden estable
  ya calculado, absorbe sus arenas en la arena de la unidad, fusiona mapas y hace commit/procesado.
- `AstArena::absorb` transfiere propiedad sin cambiar direcciones de los objetos.

## Invariantes

- Ningún worker escribe en `ProjectSyntaxUnit::arena()`.
- Ningún worker escribe en los mapas globales de tipos resueltos.
- El orden de commit coincide con el orden estable de la onda.
- La cantidad de tareas nunca excede el `-j` efectivo.
- Un producto rechazado destruye su arena privada y no deja nodos colgados en la unidad.
- Un producto aceptado transfiere una sola vez todos sus objetos a la arena de la unidad.

## Validación permitida

1. Construcción focal de `nsc` y de los targets de prueba afectados.
2. Prueba unitaria de transferencia de propiedad de `AstArena`.
3. Caso host focal de escala genérica con varios workers.
4. `--check-project-generics` sobre fixtures propios, incluido `generics-32`, con `-j 16`.
5. Repetición focal para buscar inestabilidad.

No se ejecutan baseline, deploy, instalación ni batería completa. El cierre definitivo del incidente
queda pendiente hasta que el usuario reconstruya/instale y repita el proyecto externo original.

## Evidencia local de cierre técnico

- `nsc` focal: compiló correctamente; contrato de runtime estático PASS.
- Build corregido final: `b63b828b21bb59af`.
- `TA-AST-006`: PASS. Conserva punteros al absorber la arena, la arena origen queda vacía y el destino
  destruye el objeto transferido exactamente una vez.
- `TF-FRONT-168`: PASS. Carga genérica de 32 ramas, cadena de 16 niveles y cuatro workers.
- `NativeSharp.Core.Collections.Generic`: PASS de `--check-project-generics` con `-j 16`;
  59 plantillas y 49 especializaciones.
- `generics-32`: PASS 100/100 ejecuciones consecutivas con `-j 16`.

Estas pruebas demuestran el cierre de las carreras y del fanout sin límite encontrados en G09. No
demuestran todavía que el proyecto externo original esté reparado, porque sus fuentes y su conjunto
de metadata no están disponibles en esta máquina. Ese último gate debe comparar el nuevo build hash
y confirmar que el proceso publica `generics finished` sin `0xC00000FD`.

La auditoría posterior encontró y corrigió por separado el desbalance causal de las pre-reservas de
una onda. Véase `TANDA-CORRECTIVA-NSC305-G09-BUDGET-WAVE.md` para la reproducción contrafactual,
el cambio y su evidencia focal. El gate externo permanece sin cambios.
