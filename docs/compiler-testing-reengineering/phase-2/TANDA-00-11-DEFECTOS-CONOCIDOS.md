# Tanda 00.11 — Resultados de defectos conocidos

Fecha: 2026-10-09

## Propósito

Separar tres conceptos que no son equivalentes:

- **implementado**: el caso existe, conserva su ID y ejecuta su oráculo real;
- **verificado**: el producto satisface el oráculo y el caso produce `PASS`;
- **defecto conocido**: el caso existe y demuestra que el producto todavía incumple
  el oráculo, por lo que produce `KNOWN_FAILURE` con evidencia.

Una prueba roja no se elimina, no se convierte en éxito y no reduce el denominador.

## Contrato de ejecución

- El manifiesto admite el campo opcional `expected_outcome`; su valor normal es
  `PASS` y el único valor alternativo es `KNOWN_FAILURE`.
- Cada caso de defecto conocido se ejecuta individualmente por el controlador. El
  proceso aislado conserva stdout, stderr, duración y código de salida.
- Un `FAIL` coherente se publica como `KNOWN_FAILURE`, nunca como `PASS`.
- Un caso registrado como defecto que empieza a pasar produce `UNEXPECTED_PASS` y
  obliga a retirar la cuarentena y actualizar el tracking.
- Un crash del proceso aislado puede caracterizar un defecto conocido; un timeout,
  un error de lanzamiento o una salida incoherente siguen siendo errores de
  infraestructura.
- `--all` y `--component` del host consolidado excluyen los casos de defecto para
  impedir que comportamiento indefinido derribe el flujo ordinario. `--case`
  conserva su ejecución explícita.

## Alcance

Esta tanda sólo añade la representación y el aislamiento. No materializa todavía
los siete casos de producto, no modifica `include/` ni `src/`, no ejecuta la suite
histórica y no cambia el conteo acumulado de la ola 1.

## Gate de cierre

```text
casos previstos ola 1 = 747
casos implementados = 736
casos verificados PASS = 736
casos KNOWN_FAILURE materializados = 0
casos de defecto por materializar = 7
casos de infraestructura especial pendientes = 4
```

