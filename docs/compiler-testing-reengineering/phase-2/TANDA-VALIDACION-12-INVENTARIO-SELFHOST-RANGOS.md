# Tanda de Validación 12 — Inventario self-host por rangos

## Alcance causal

Se migraron al inventario vivo siete gates cuyos módulos materializan rangos de
casos:

- Driver;
- IR;
- Parser;
- Projects;
- Sema;
- Symbols;
- Syntax.

## Corrección

Los gates dejaron de copiar en Python:

- rangos ordinales;
- cantidades de contratos;
- repartos por capa;
- cantidades PR/NIGHTLY;
- tamaños de los arreglos NIGHTLY de los headers.

Los rangos esperados se construyen desde `test-case-design.csv`. Los perfiles
NIGHTLY esperados se construyen desde `suite-case-allocation.csv` y se cotejan
contra los ordinales realmente materializados en cada header. Por tanto, una
alta legítima requiere materializar su caso, pero no actualizar un número
duplicado en el gate.

## Defecto del controlador encontrado

`SelfHostProjectsIdentityModule.cpp` contiene el contrato `CSH-PRJ-005`, pero
su texto de oráculo comienza en la línea siguiente. La expresión regular de
`run_selfhost_projects_gate.py` exigía que comenzara en la misma línea y omitía
silenciosamente todo el descriptor.

El parser acepta ahora espacio, incluido salto de línea, antes del oráculo. No
se cambió el módulo, el contrato ni el caso.

## Evidencia

Los siete gates estáticos terminaron con código cero después de cotejar rangos,
capas, perfiles, disposiciones y ausencia del manifiesto ejecutable:

| Gate | Resultado |
|---|---|
| Driver | PASS |
| IR | PASS |
| Parser | PASS |
| Projects | PASS |
| Sema | PASS |
| Symbols | PASS |
| Syntax | PASS |

No se configuró, compiló ni ejecutó producto. El resultado demuestra coherencia
de infraestructura, no éxito funcional de casos.

## Límites preservados

- módulos y headers C++ intactos;
- casos, contratos, oráculos y disposiciones intactos;
- batería histórica y archivos protegidos intactos;
- sin baseline, `C:`, despliegue ni `push`.
