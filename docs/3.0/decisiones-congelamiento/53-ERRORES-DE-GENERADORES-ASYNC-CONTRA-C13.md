# Congelamiento 3.0, elemento 134 — `async-generator-errors.md` revisado contra C-13

Fecha: 2026-10-26. Revisión de partida: `54753728f`.

## Nota de trabajo

```text
HEAD: 54753728f
Elemento: 134 del plan de 148 (R-2x de la auditoría: «async-generator-errors.md: quitar "C-13 exige declararlo"»)
Contrato vigente: C-13 / P-1 (toda llamada que declara throws, todo throw y todo await con error que escapan se
  atrapan o se declaran), conservado por Alberto el 2026-10-26 (doc 03); DEC-14 (typed-throws.md, doc 33)
Documento histórico que podría contradecirlo: la recomendación de la auditoría, escrita cuando C-13 iba a
  retirarse; NSC-DECISIONES-VIGENTES §6.4 la condiciona a la conducta medida
Archivos exactos que se modificarán:
  docs/3.0/language/async-generator-errors.md
  docs/3.0/decisiones-congelamiento/53-ERRORES-DE-GENERADORES-ASYNC-CONTRA-C13.md
```

## Qué se revisó

Revisé cada afirmación del documento contra el compilador actual con las sondas `A:\temp\nsc-dec\d39`, en `-O0`
y `-O2`. Todas compilan como proyecto implícito y dan el mismo resultado en los dos niveles. `Gen` es un
`AsyncIterator<int, MyError>` que declara `throws MyError`, entrega 7 y después lanza `MyError(37)`; tiene un
`raii Guard(1)` vivo en el `yield`.

| Afirmación | Sonda | Resultado |
|---|---|---|
| Reanuda después del `yield`; nada anterior se repite | E6ResumeOnce | 3111: cada tramo corre una vez |
| La limpieza activa corre una vez antes del error | E2Declared | 10371: `Guard(1)` una vez; el `catch` recibe 37 |
| Terminado: un `MoveNextAsync` posterior responde `false` | E3CaughtInside | 7371 |
| El `catch` de un llamador async recibe el mismo objeto | E7AsyncCaller | 371 |
| Al propagar se destruyen los locales del consumidor | E8ConsumerLocals | 3712: `Guard(1)` y luego `Guard(2)` |
| C-13: el `await` sin `try` ni `throws` se rechaza | E1Undeclared | rechazada en los dos `await` |
| C-13: el generador declara lo que lanza | E4GeneratorUndeclared | rechazada en el `throw` |
| Un `E` que no es excepción no exige declarar | E5NonExceptionError | 7 |
| `Cancel()` y luego `MoveNextAsync`: limpieza y `false` | E9CancelParked | 51 |
| Destruir un iterador detenido corre la limpieza una vez | E10DropParked | 71 |
| Un iterador que nunca empezó no ejecuta nada | E11NeverStarted | 50 |
| `Main` que declara el error compila | E12MainDeclares | compila; no se ejecuta |

## Decisión

- **«C-13 exige declararlo».** La conducta medida coincide con el contrato vigente, así que la frase no se retira.
  Se precisa:
  - la exigencia vale para un `E` que es excepción;
  - también el generador declara lo que lanza;
  - un `E` que no es excepción no produce error.
- **«Desde `Main` termina el proceso con la excepción nativa».** Se conserva con su condición (un `Main` que
  declara el error y no lo atrapa) y con el código `0xE04E5301`, observado en la corrida de NSC Builder 3
  `20261025-222449-571bf9f4` (doc 03). No lo ejecuté aquí, porque el reporte de errores de Windows demora esa
  ruta (DEC-15).

## SCOPE LOCK

- **Cambia.** Sólo el párrafo de propagación de `async-generator-errors.md` (ES y EN) y la línea de referencias.
- **Idéntico.** El compilador, las pruebas y el resto del documento.

## No verificado

- La terminación del proceso por un error que sale de `Main`, en esta revisión (ver arriba).
- Batería completa y unitarias (`testAsyncGeneratorErrors`, `testAsyncGeneratorResume`): gate final.
