# NS304-T05-F04 — Frame layout, discriminante, liveness y storage de locals suspendidos

- **Estado**: ABIERTO (auditado; liveness pendiente)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *calcular frame layout, discriminante, liveness y storage de locals suspendidos*.

Estado real tras F03 (cerrado):

- **Frame layout**: HECHO en F03. `ProjectOopMirModel` publica el layout uniforme
  `future:<T> = { i32 state@0; void* frame@8; T result@16 }` y, para cada poll, `asyncFrameSize`
  = suma de los tamaños de los futures esperados (los `AwaitFuture` del cuerpo).
- **Discriminante**: HECHO en F03. `state` con encoding uniforme `0` created, `-1` ready,
  `1..N` resume (suspend tras await i). El `await` chequea `child.state == -1` sin conocer el
  conteo del hijo.
- **Storage**: HECHO en F03 (caller-stack). El `await` (sync y async) aloca el frame del productor
  esperado y lo guarda en `future.frame@8`; no hay heap implícito.
- **Liveness**: NO HECHO. Sólo los futures hijo se guardan en el frame. Un **local que cruza el
  await** (definido antes, usado después) NO se guarda/restaura.

## Gap demostrable

`async int F(){ int y = 10; int x = await G(); return x + y; }` — `y` está vivo a través del await.
Hoy el suspend/resume guarda sólo el future hijo; `y` se pierde al reanudar (el stack del poll se
destruye al suspender) y `x + y` lee basura. Es un resultado incorrecto silencioso.

**Acoplamiento**: el defecto SÓLO se observa cuando el `await` suspende de verdad. Con F03 los
futures completan sincrónicamente (no hay primitiva pendiente), así que el gap de liveness no se
ejecuta en runtime todavía. La primitiva pendiente es F09.

## Plan

1. Computar la liveness por await: qué locals (ordinales) están vivos a través de cada `AwaitFuture`
   (definidos antes, usados después), y asignarles slot en el frame (tras los futures hijo).
2. Ampliar el frame por-función: `asyncFrameSize` = futures hijo + slots de locals vivos.
3. En el suspend/resume (`AwaitFuture`), guardar/restaurar los locals vivos a/desde el frame,
   junto al future hijo.

## Prueba puntual

`async int F(){ int y = 10; int x = await G(); return x + y; }` debe conservar `y` a través del
await y devolver `x + 10` tras una suspensión real (necesita la primitiva pendiente de F09 para
ejercer el camino).

## Nota

F04 depende de F09 para la ejecución runtime (la suspensión real); sin F09 el liveness es un
transform no ejercido. Se implementará el transform aquí y se validará la ejecución cuando F09
aporte la primitiva pendiente.
