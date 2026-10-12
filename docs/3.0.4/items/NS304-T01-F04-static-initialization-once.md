# NS304-T01-F04 — inicialización estática once y reentrante

## Contrato

Cada tipo con inicialización estática productiva posee un coordinador único. El
primer hilo reclama el estado `uninitialized -> initializing`, ejecuta primero
los inicializadores de campos y después el cuerpo explícito `static Tipo()`, y
publica `initialized` sólo al terminar. Un hilo competidor espera; una entrada
recursiva del mismo hilo no se espera a sí misma.

El ciclo entre módulos se rechaza en `ProjectGraphLoader` antes de producir el
plan dependency-first. F04 no define todavía el estado terminal de una
excepción ni la política de panic: esos contratos pertenecen a F05.

## Implementación

- El cuerpo declarado conserva identidad propia `__nsc_cctor_body_*`; el
  coordinador privado conserva `__nsc_cctor_*`.
- `ProjectOopLlvmLowerer` crea coordinador tanto para inicializadores dinámicos
  de campos como para un constructor estático explícito.
- El coordinador usa carga y `cmpxchg` secuencialmente consistentes, publica el
  identificador del hilo propietario, espera en `cctor.spin_wait` a otros hilos
  y permite la reentrada propia mediante `cctor.is_self`.
- El cuerpo explícito se llama después de los inicializadores de campos y antes
  de publicar estado `2`. El acceso estático y la entrada a constructores de
  instancia pasan por el mismo coordinador.

## Evidencia puntual

- `TL-LANG-297`: `PASS`, 68613 microsegundos; distingue cuerpo MIR y coordinador
  y rechaza un ciclo explícito `Producer <-> Consumer`.
- `TL-LANG-296`: `PASS`, 11406 microsegundos.
- `TL-LANG-295`: `PASS`, 45586 microsegundos.
- `TL-LANG-160`: `PASS`, 1268 microsegundos; la promoción pública sigue
  pendiente hasta cerrar la vertical F08.
- `nsc_stage37_static_initialization_tests`: `PASS`, 61 checks; verifica en IR
  `cmpxchg -> body -> publish`, espera entre hilos, reentrancia del propietario,
  verificador LLVM, optimización O2 y objeto COFF AMD64.

La validación reconstruyó la clausura ABI exacta de 35 objetos consumidores de
`ProjectMemberModel.h` y el `Parser.cpp.obj` de F02 antes de enlazar Stage37; el
primer `0xC0000005` quedó demostrado como mezcla de objetos stale, no como
resultado del producto F04.

## Gate F04

```text
claim = atomic-cmpxchg(0, 1)
winner = field-initializers -> explicit-body -> publish(2)
other-thread = spin-until(2)
same-thread-reentry = bypass-wait
module-cycle = rejected-before-plan
failure-terminal = deferred-to-F05
```
