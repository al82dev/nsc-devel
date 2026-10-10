# Tanda de Validación 26: registro global contra inventario

Fecha: 10 de octubre de 2026
Estado: IMPLEMENTADA; EJECUCIÓN OFFLINE PENDIENTE

## Omisión cerrada

Antes de esta tanda no existía una comprobación global de `--list`. Los gates
de `Host.Source` y `Host.nsc` verificaban sólo sus componentes; una omisión de
cualquier otro módulo podía sobrevivir hasta una corrida funcional tardía.

`run_registry_inventory_gate.py` deriva el registro esperado desde:

- `test-case-design.csv` para las familias `Host.*` y `SelfHost.*`;
- los inventarios y el manifiesto del fixture sintético `Infrastructure`.

Después ejecuta `nsc_component_tests --list` y exige igualdad exacta y ordenada
de `(case_id, contract_id, component)`. Reporta por separado casos faltantes,
inesperados y casos cuya identidad conserva ID pero cambia contrato o componente.

## Evidencia estática

- Filas esperadas derivadas: `9,116`.
- Host: `7,360`; SelfHost: `1,753`; Infrastructure: `3`.
- Componentes esperados: `43`.
- Parser probado contra round-trip completo, línea mal formada, duplicado y
  desorden: `2/2` PASS.
- La auditoría CMake confirma una referencia exacta del nuevo controlador y
  unicidad del nuevo nombre CTest.
- `py_compile` del gate y su prueba: PASS.

La evidencia de ejecución no se anticipa: los hosts encontrados en A: fueron
construidos el 9 de octubre y contienen sólo `1,076`, `1,076` y `732` filas,
por lo que son artefactos obsoletos y no certifican el árbol actual. El gate se
ejecutará con el host regenerado por el usuario.

No se modificaron módulos, casos, oráculos, disposiciones, producto ni batería
histórica. No se configuró ni compiló producto; no hubo baseline ni despliegue.
