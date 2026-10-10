# Tanda de validación 153 — Presupuesto NIGHTLY de Dominance

## Estado

`IMPLEMENTADA; VALIDACIÓN INTEGRAL DIFERIDA HASTA VALIDACIÓN 155`

## Defecto concreto

Once casos adversariales de `Host.Dominance` agotaron exactamente el límite de
60 segundos y quedaron como error de infraestructura. Todos pertenecen al
perfil `NIGHTLY` y ejercitan grafos profundos o matrices de escala elevada.

## Causa

Las once filas conservaban `timeout_class: medium`, el mismo presupuesto usado
por cargas ordinarias. El manifiesto ya posee la clase canónica `extended` de
180 segundos para cargas nocturnas largas; estas filas no la declaraban.

## Corrección mínima

Sólo se cambia a `extended` el contrato temporal de:

`TF-DOM-024`, `032`, `040`, `072`, `080`, `104`, `128`, `216`, `224`, `232`
y `256`.

No se reducen tamaños, iteraciones, grafos, matrices ni comprobaciones. No se
modifica Dominance ni se transforma un timeout eventual en éxito: superar 180
segundos seguirá publicándose como error de infraestructura con evidencia.

## Evidencia preparada

La prueba dirigida exige la presencia exacta de los once IDs, su perfil
`NIGHTLY` y su clase `extended`. Por instrucción expresa su ejecución se
difiere hasta la corrida única final `Debug/O0/Todos`.
