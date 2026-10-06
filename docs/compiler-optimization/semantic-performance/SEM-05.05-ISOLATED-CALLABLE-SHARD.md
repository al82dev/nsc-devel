# SEM-05.05 — Shard aislado por callable

## Propósito

Ejecutar la unidad semántica de un callable sobre un `ProjectMethodBodyModel` privado, sin
escribir en los vectores, índices ni scratch del modelo coordinador.

## Flujo

1. `createCallableShard` conecta sólo catálogos inmutables del contexto.
2. El catálogo de adapters se comparte mediante el span congelado de SEM-05.04.
3. El shard usa un `DiagnosticEngine` local sin límite prematuro.
4. `analyzeCallable` escribe exclusivamente dentro del shard.
5. El shard y sus diagnósticos se mueven a `ProjectCallableAnalysisResult`.
6. Llamadas, delegates, optional y Result publican descubrimientos globales estables.
7. Los descubrimientos se ordenan y deduplican antes de regresar al coordinador.

## Invariantes

- ningún output parcial se agrega al modelo global;
- no se copian adapters de funciones libres;
- el shard no ejecuta el cierre interprocedural de escapes;
- el límite de diagnósticos se aplicará una sola vez durante la fusión global;
- no hay recursión ni estado estático compartido.

## Archivos

- `ProjectMethodBodyModel.h/.cpp`;
- este documento.

## Estado de integración

Este commit crea y ejecuta la ruta aislada; el coordinador serial seguirá usando la ruta previa
hasta que SEM-05.06 aterrice remapeo y merge de todos los outputs. Esa separación evita publicar
un merge parcial o cambiar ordinales antes de que el remapper esté completo.

## Validación dirigida

Build con `/WX` del objetivo `nsc_project_method_body_model_tests`. La prueba conductual de
merge se agrega junto con SEM-05.06. No baseline, minibaseline, CTest general ni O1/O2/O3.
