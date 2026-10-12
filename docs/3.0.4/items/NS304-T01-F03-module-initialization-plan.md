# NS304-T01-F03 — plan de inicialización de módulos

## Contrato

Un proyecto es el módulo semántico de inicialización. El plan conserva el orden
topológico dependency-first de `ProjectGraph`, publica las dependencias directas
por ordinal de proyecto y agrupa, sin ejecutar, los constructores estáticos que
pertenecen a cada módulo.

F03 no añade sintaxis `module initializer`: la declaración productiva continúa
siendo el constructor estático de tipo cerrado en F02. Tampoco implementa el
once thread-safe, espera, reentrancia, ciclos ni fallo terminal; esas transiciones
pertenecen a F04/F05.

## Implementación

- `ProjectMemberModel::buildModuleInitializationPlan` valida la correspondencia
  uno-a-uno entre proyectos y espacios de declaración.
- Cada `ModuleInitializationRecord` conserva `projectOrdinal`, `moduleName`, las
  dependencias directas deduplicadas y ordenadas, y los ordinales de los
  constructores estáticos propios.
- `ProjectCompilation::compileProjectMembers` construye el plan después del
  modelo de miembros. Cargar un `ProjectReference` sólo programa el módulo; no
  ejecuta ni reasigna sus inicializadores.

## Evidencia puntual

- Productor: `nsc_component_tests`, Debug/O0, árbol ya configurado.
- `TL-LANG-296`: `PASS`, 40707 microsegundos. Dos proyectos, arista
  `Consumer -> Producer`, orden dependency-first, dependencia directa y
  ownership exactos.
- Guard `TL-LANG-295`: `PASS`, 141701 microsegundos.
- Guard `TL-LANG-160`: `PASS`, 4306 microsegundos; la superficie completa sigue
  `PlannedFor304` hasta F08.

## Gate F03

```text
module_identity = project
order = dependency-first
direct_dependencies = explicit
static_constructor_ownership = exact
execution_on_import = none
thread_safe_once = deferred-to-F04
```

