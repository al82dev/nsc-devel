# Tanda de Validación 16 — Denominadores host por componente

## Alcance causal

Quince gates host repetían rangos de IDs, contratos ejecutables, exclusiones,
perfiles, capas o cantidades propias de una revisión. Esa duplicación podía
rechazar una matriz correcta después de agregar o retirar casos canónicos.

## Correcciones

1. Un helper común carga el manifiesto y los inventarios mediante
   `InventoryIndex`, `load_manifest` y `select_cases`, las mismas funciones que
   usa `run_suite.py`.
2. Cada gate obtiene su denominador con componente, perfil y build mode vivos.
3. El cierre compara exactamente el orden de resultados contra la selección y
   deriva los conteos de `expected_outcome`; no contiene listas paralelas de
   defectos conocidos.
4. El gate compuesto de IR aplica el mismo contrato por separado a HIR, CFG,
   Dominance, MIR, SSA y Passes.
5. `Host.nsc` deriva sus pares caso/contrato del inventario canónico y conserva
   vacío el fragmento ejecutable mientras sus casos sigan pendientes.

## Evidencia dirigida

- `25/25` pruebas Python de manifiesto, selector y helper: PASS.
- validación sintáctica de los dieciséis módulos Python modificados: PASS.
- selección canónica Debug: `19` componentes, `4,774` filas ejecutables.
- búsqueda residual: sin rangos, IDs ni totales de revisión en los gates
  migrados; permanecen únicamente cardinalidades semánticas y códigos de salida.

## Límites preservados

- sin ejecución de `nsc_component_tests` ni producto;
- sin configuración, compilación, baseline, `C:`, despliegue ni `push`;
- ningún caso, oráculo o resultado esperado modificado;
- batería histórica y seis archivos protegidos intactos.
