# SCOPE LOCK — Fase 2

## Objetivo

Implementar la suite nueva especificada por la Fase 1, siguiendo las olas y dependencias publicadas. La unidad de
trabajo es el contrato observable y cada caso conserva trazabilidad `case_id -> contract_id -> component`.

## Invariantes

- Las 684 pruebas históricas y su infraestructura permanecen intactas y disponibles como referencia semanal.
- Los seis cambios preexistentes registrados por la Fase 1 no se modifican ni se incluyen en commits.
- No se cambia el comportamiento del compilador para satisfacer una prueba.
- No se toca `C:`, no se despliega y no se usan remotos.
- No se implementa cobertura instrumental en esta fase.
- Un defecto observable conserva una prueba real con oráculo correcto aunque permanezca roja hasta corregir el
  producto; queda prohibido convertirlo en `SKIP`, `XFAIL`, éxito esperado u oráculo vacío.
- Una expectativa de prueba que no corresponde a una capacidad vigente del compilador es un defecto del test
  plan: se elimina o se reescribe contra el contrato real. No se conserva como feature pendiente dentro de la
  suite activa ni autoriza ampliar el producto.
- Una prueba válida que requiere infraestructura especial se registra como pendiente explícito. No se declara
  verificada, pero tampoco bloquea contratos u olas causalmente independientes.
- Avanzar trabajo independiente no reduce denominadores, no oculta pendientes y no autoriza declarar completa
  una ola que aún contenga casos sin verificar.
- Un productor fallido bloquea a sus consumidores; no se usan artefactos viejos como fallback.

## Orden

1. Ola 0: infraestructura mínima y fixtures sintéticos del framework.
2. Olas 1 a 6: compilador host por dependencias.
3. Olas 7 y 8: self-host y bootstrap.
4. Ola 9: construcción y paquete del compilador.

Dentro de la Fase 2 se aplica una separación adicional autorizada el 9 de octubre de 2026:

1. materializar e integrar primero todos los casos, manifiestos y superficies de ejecución de las olas;
2. conservar durante esa implementación todos los defectos y limitaciones con estado visible;
3. no gastar CPU en compilaciones, ejecuciones, gates ni ciclos de validación parciales;
4. ejecutar la validación offline sólo después de completar la infraestructura prevista.

Esta separación cambia el momento de la validación, no reduce la implementación ni permite presentar como
verificado aquello que todavía no se ejecutó.

La Fase 2 es exclusivamente la suite de NSC. Las bibliotecas `Library.*`, el SDK
y los componentes de otros repositorios no se implementan, registran ni ejecutan
en esta infraestructura. Sus inventarios de Fase 1 conservan valor documental,
pero cualquier suite futura les pertenece a sus repositorios respectivos.

Cada cambio incluye implementación, pruebas nuevas enfocadas, evidencia Markdown y un commit pequeño.
El mismo commit que cierra una tanda actualiza `TRACKING.md` y `CURSOR.json`; queda prohibido diferir el estado
persistente a una tanda posterior.
