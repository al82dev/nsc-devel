# NS304-T07-F07 — Send/Sync, thread y executor

- **Estado**: AUDITADO (sin Send/Sync ni executor; surface + type-system pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar Send/Sync y executor por protocolo*.
Plan T07-F07 (DeepSeekDocs): *resolver constraints de frame, captures, waker e item al cruzar thread;
un executor se selecciona mediante protocolo explícito; rechazar transferencia de storage no Send/Sync;
probar executor single-thread y multithread controlado*.

Estado real (tras F02):

- **Sin Send/Sync**: no hay marcadores de tipos ni verificación de transferencia entre threads.
- **Sin executor**: el async-generator baja a caller-stack (T05-F08) sin selección de executor; no hay
  protocolo explícito.

## Gap

No hay constraints Send/Sync que rechacen la transferencia de un frame/capture/item no seguro entre
threads, ni un protocolo de executor que el usuario elija (sin scheduler global).

## Decisión de surface (material, para Alberto)

- **(A) marcadores `Send`/`Sync` en el tipo** (recomendada): `Send T` / `Sync T` explícitos; el
  compilador rechaza cruzar un thread con storage no Send/Sync.
- **(B) traits implícitos** (auto Send/Sync por composición).

Recomendación: **(A)**.

## Plan (tras surface)

1. Resolver `Send`/`Sync` + verificar el frame/captures/item del async-iterator al cruzar thread.
2. Protocolo de executor (`IExecutor`/`scope.Executor`) que el usuario selecciona; el lenguaje no
   instala scheduler global.
3. Probar single-thread y multithread controlado (executor elegido).

## Nota

F07 depende de la surface Send/Sync + F06 (scope) y F05 (cancelación). Es un vertical grande que
toca el type-system (constraints) y la compilación/scheduler sólo donde el contrato lo consuma.
