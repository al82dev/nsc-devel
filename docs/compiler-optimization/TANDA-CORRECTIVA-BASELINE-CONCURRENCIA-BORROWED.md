# Tanda correctiva — arrays no propietarios en concurrencia

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64
**Validación:** dirigida O0; baseline no ejecutado

## Problema observado

Los gates 371, 373 y 375 compilaban sus ejecutables O0/O2, pero los seis procesos terminaban con
`0xC0000374` (`STATUS_HEAP_CORRUPTION`). Las sondas O0 confirmaron que todo el trabajo, espera,
cancelación, stealing y shutdown concluía correctamente; `Environment.Exit(42)` después del
shutdown evitaba el fallo. La corrupción aparecía únicamente durante el cleanup RAII posterior a
`Main`.

La causa común era que `delete[]` descartaba el `storageOwnership` calculado por semántica y
destruía siempre cada elemento reference-like:

- `ThreadPool.queues_` contiene aliases repetidos al mismo `ThreadStart`; destruir cada slot
  liberaba varias veces la misma celda delegate y además memoria propiedad del caller o del Job.
- `Job.dependencies_` contiene referencias prestadas a otros jobs; destruir los elementos desde
  el dependiente duplicaba la destrucción de sus owners RAII.

## Cambios

- `ProjectOopHirModel.cpp`: `delete[]` transporta el ownership de almacenamiento del operando en
  vez de inventar `Owned`.
- `ProjectOopLlvmLowerer.cpp`: sólo un array `owned T[]` destruye sus elementos; un array plano o
  `borrowed T[]` libera el bloque, pero no los objetos referenciados por sus slots.
- `ThreadPool.ns`: la cola se declara como `ThreadStart[]` plano; el pool conserva ownership
  manual del buffer, no de los delegates encolados.
- `Job.ns`: las dependencias permanecen como `IJob[]` plano; el Job sólo conserva aliases.
- tests 371/373/375: materializan el resultado antes de construir el mensaje para que el código de
  salida o diagnóstico no vuelva a perderse por orden de evaluación de argumentos C++.

## Mejora

El cleanup refleja los dos ejes del contrato T41: ownership del bloque y ownership de sus slots.
Los pools y jobs dejan de liberar memoria ajena o repetida, sin convertir delegates/dependencias
prestadas en owners ocultos.

## Validación realizada

- reconstrucción Debug/O0 de `nsc`, de los tres targets dirigidos y de los dos targets Stage 48
  pendientes de confirmar en esta configuración;
- recompilación individual O0 de pool, jobs y work stealing con
  `A:\nsc-devel\build-lsp-debug\bin\nsc.exe`;
- los tres binarios terminaron con resultado `42`;
- `nsc.ns3.t41.ownership`: `1/1` passed; fixture positiva y tres negativas, todas O0;
- sin O1/O2/O3 y sin baseline.
