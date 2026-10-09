# Tanda 01.91 — defectos conocidos ejecutables

## Alcance

Esta tanda materializa los siete casos de la ola 1 cuyo oráculo correcto revela
un defecto vigente del compilador. No modifica producto, no reduce el denominador
de 747 casos y no convierte el comportamiento defectuoso en éxito.

| Caso | Contrato | Estado esperado | Defecto revelado |
|---|---|---|---|
| `TL-LANG-019` | `CL-LANG-002` | `KNOWN_FAILURE` | overflow en layout de agregado |
| `TL-LANG-035` | `CL-LANG-003` | `KNOWN_FAILURE` | overflow en layout de closure |
| `TL-LANG-099` | `CL-LANG-008` | `KNOWN_FAILURE` | enum externo indexa fuera de rango |
| `TL-LANG-121` | `CL-LANG-009` | `KNOWN_FAILURE` | dominancia asimétrica con duplicados |
| `TL-LANG-230` | `CL-LANG-017` | `KNOWN_FAILURE` | parse parcial y output parcial tras error |
| `TL-LANG-250` | `CL-LANG-019` | `KNOWN_FAILURE` | familia externa indexa fuera de rango |
| `TL-LANG-262` | `CL-LANG-020` | `KNOWN_FAILURE` | overflow en alineación final de variant |

## Política de ejecución

- Cada caso existe en el manifiesto y en su módulo real.
- `--case` puede ejecutarlo y conservar evidencia del defecto.
- `--all` y `--component` excluyen únicamente los casos con disposición
  `KnownProductDefect`, de modo que un defecto conocido no interfiere con el
  flujo ordinario.
- Los gates de cada contrato sí ejecutan su caso rojo y exigen exactamente un
  resultado `KNOWN_FAILURE`.
- `TL-LANG-099` y `TL-LANG-250` se aíslan en un proceso individual porque el
  producto vigente puede incurrir en acceso fuera de rango.

## Conteo

La ola 1 queda en `743/747` casos implementados: `736 PASS`, siete defectos
conocidos implementados y cuatro casos pendientes de infraestructura especial.

## Validación

La validación integrada se ejecutará después de este commit, conforme a la regla
de preservar primero el estado reproducible. El resultado se añadirá en un
commit posterior sin reescribir esta evidencia.

La primera validación reveló que el gate de `CL-LANG-009` todavía omitía
`TL-LANG-116`, aunque ese caso ya había sido corregido e implementado. La lista
se cambió a la secuencia canónica `TL-LANG-112..123`; no se modificó ningún caso
ni se redujo el contrato para hacer pasar el gate.
