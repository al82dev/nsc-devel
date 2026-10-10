# Tanda de Validación 11 — Inventario self-host común, grupo 1

## Alcance causal

Se eliminó la duplicación del denominador de cuatro controladores self-host:

- `SelfHost.Bootstrap`;
- `SelfHost.CodeGen`;
- `SelfHost.Documentation`;
- `SelfHost.Project`.

La causa era la misma que en la tanda anterior: rangos, cantidades, repartos
por contrato y distribuciones de capa/perfil congelados dentro de los gates.

## Implementación

`selfhost_inventory.py` concentra exclusivamente la lectura y las invariantes
relacionales compartidas:

1. fuentes canónicas no vacías;
2. IDs de caso y contrato únicos;
3. paridad exacta entre diseño y asignación;
4. contrato y capa estables por caso;
5. todos los contratos referenciados y ningún contrato huérfano;
6. rangos materializados derivados del diseño vivo;
7. ausencia de casos pendientes en el manifiesto ejecutable.

Cada gate conserva sus comprobaciones propias de disposición y materialización.
CodeGen sigue cotejando además el oráculo publicado por el contrato. Bootstrap
conserva capa y perfil por caso. Documentation y Project derivan la existencia
de sus contradicciones de la diferencia real entre `frequency` y
`default_profile`.

## Evidencia

Los cuatro gates se ejecutaron directamente en modo estático contra el árbol
actual y terminaron con código cero:

| Gate | Resultado |
|---|---|
| `run_selfhost_bootstrap_gate.py` | PASS |
| `run_selfhost_codegen_gate.py` | PASS |
| `run_selfhost_documentation_gate.py` | PASS |
| `run_selfhost_project_gate.py` | PASS |

La sintaxis Python y `git diff --check` pasaron. No se configuró, compiló ni
ejecutó NSC o un fixture de producto, por lo que esta evidencia no se promueve
a validación funcional.

## Límites preservados

- módulos C++ y proyectos `.nsproj` intactos;
- casos, contratos, oráculos y disposiciones intactos;
- batería histórica intacta;
- seis archivos protegidos intactos;
- sin baseline, despliegue, escritura en `C:` ni `push`.
