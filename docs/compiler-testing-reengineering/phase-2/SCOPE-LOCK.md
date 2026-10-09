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
- No se avanza de ola con casos pendientes, `SKIP` temporales u oráculos vacíos.
- Un productor fallido bloquea a sus consumidores; no se usan artefactos viejos como fallback.

## Orden

1. Ola 0: infraestructura mínima y fixtures sintéticos del framework.
2. Olas 1 a 6: compilador host por dependencias.
3. Ola 7: bibliotecas y SDK mediante el compilador real.
4. Olas 8 y 9: self-host y bootstrap.
5. Ola 10: construcción y paquete.
6. Ola 11: disposición externa sin casos propios de NSC.

Cada cambio incluye implementación, pruebas nuevas enfocadas, evidencia Markdown y un commit pequeño.
