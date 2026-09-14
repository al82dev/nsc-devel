# 317 — Proyectos fachada sin redeclaración / Source-less project facades without redeclaration

## Resumen / Summary

### Español

Se corrigió la propiedad de fuentes en grafos con `ProjectReference`. Un proyecto fachada puede contener cero fuentes propias y agregar uno o más proyectos sin volver a declarar bajo su módulo los tipos que ya pertenecen a las dependencias.

El caso mínimo corregido es:

```text
A.nsproj (AMod) -- declara T
        ↑
U.nsproj (UMod) -- sin Compile; sólo referencia A
        ↑
C.nsproj (CMod) -- usa T
```

Antes, el glob implícito `**/*.ns` de `U` podía volver a recoger físicamente las fuentes de `A`. El frontend recibía dos unidades para el mismo archivo y construía `AMod::T` y `UMod::T`, por lo que `C` diagnosticaba un nombre ambiguo. Ahora cada fuente conserva un único propietario: el proyecto dependiente no vuelve a apropiarse de archivos que ya están en la clausura de una dependencia.

### English

Source ownership in `ProjectReference` graphs was corrected. A facade project may contain no sources of its own and aggregate one or more projects without redeclaring dependency types under the facade module.

Previously, `U`'s implicit `**/*.ns` glob could physically collect `A`'s sources again. The frontend then received two units for the same file and built both `AMod::T` and `UMod::T`, making `T` ambiguous in `C`. Every source now retains one owner: a dependent project cannot reclaim files already present in a dependency closure.

## Semántica / Semantics

1. El grafo sigue ordenándose topológicamente: dependencias antes que consumidores.
2. Para cada proyecto se calcula la clausura de fuentes de sus `ProjectReference` transitivos.
3. Una fuente propia que ya existe en esa clausura se elimina del conjunto del consumidor.
4. Las demás fuentes del consumidor se conservan con su módulo original.
5. Una fachada cuyo conjunto queda vacío sigue siendo un nodo real del grafo y conserva sus referencias.
6. Un proyecto independiente vacío continúa siendo inválido; sólo se admite cero fuentes cuando existen `ProjectReference`.
7. La regla no combina declaraciones `partial` entre módulos ni cambia su semántica.

The graph remains topologically ordered. For each project, NSC computes the transitive source closure of its `ProjectReference` dependencies. A source already owned by that closure is removed from the consumer's own set; every other consumer source remains unchanged. An empty facade stays a real graph node with its references intact. An unrelated empty standalone project remains invalid.

## Archivos modificados / Modified files

- `include/nativesharp/Compilation/ProjectSourceSet.h`
  - `ProjectSourceSet::finalize` acepta una política explícita `allowEmpty`.
- `src/Compilation/ProjectSourceSet.cpp`
  - Conserva el rechazo histórico de proyectos vacíos por defecto.
  - Permite el conjunto vacío sólo cuando el llamador identifica una fachada válida.
- `src/Compilation/ProjectLoader.cpp`
  - Permite que un proyecto con `ProjectReference` llegue vacío al grafo.
- `src/Compilation/ProjectGraphLoader.cpp`
  - `normalizeTransitiveSourceOwnership` calcula clausuras por ruta canónica.
  - Reconstruye cada `ProjectSourceSet` sin archivos ya poseídos por dependencias.
  - Preserva nombre de módulo, tipo de salida, referencias de proyecto y referencias precompiladas.
- `tests/unit/ProjectNameLookupTests.cpp`
  - Añade el repro mínimo con tres módulos.
  - Verifica que `UMod` tenga cero fuentes, que sólo exista `AMod::SharedFacade.T`, que el consumidor resuelva `T` una vez y que toda la cadena llegue hasta OOP MIR.

## Decisiones de diseño / Design decisions

- **Propiedad por ruta física / Ownership by physical path.** La identidad se calcula con ruta canónica y comparación sin distinción de mayúsculas, coherente con Windows.
- **La dependencia gana / Dependency wins.** Si un archivo ya pertenece a un proyecto referenciado, su módulo nominal no puede cambiar sólo porque un glob más amplio lo encuentre otra vez.
- **Normalización después del orden topológico / Normalize after topological ordering.** Cada clausura se construye a partir de dependencias ya normalizadas; no se necesita una segunda carga ni se altera la detección de ciclos.
- **Sin reexportación sintética de tipos / No synthetic type re-export.** La fachada da visibilidad transitiva al módulo propietario. No clona tipos, funciones ni identidades físicas.
- **Compatibilidad con proyectos mixtos / Mixed-project compatibility.** Una fachada puede agregar dependencias y declarar fuentes propias; sólo se quitan las que ya tienen propietario transitivo.

## Evidencia focalizada / Focused evidence

```text
cmake --build build-test-lldlinker --target nsc_project_name_lookup_tests -j 8
build-test-lldlinker\tests\nsc_project_name_lookup_tests.exe
PASS: 82 checks
```

El repro recorre carga del grafo, sintaxis, genéricos, nombres, miembros, herencia, interfaces, cuerpos, dispatch virtual, destrucción, construcción, layout, OOP HIR y OOP MIR.

No se ejecutó baseline ni se compiló el runtime, bindings o tutoriales.

No baseline, runtime, bindings, or tutorials were run.

## Mensaje de commit de referencia / Reference commit message

### Título / Title

```text
Agrega proyectos fachada sin redeclarar módulos / Add project facades without redeclaring modules
```

### Descripción / Description

```text
ES:
Normaliza la propiedad de fuentes a través de ProjectReference para que un
proyecto fachada no vuelva a absorber archivos ya poseídos por dependencias
transitivas. Permite fachadas sin fuentes, conserva fuentes propias y verifica
el flujo A→U→C completo hasta OOP MIR sin tipos ambiguos.

EN:
Normalize source ownership across ProjectReference so a facade project cannot
reabsorb files already owned by transitive dependencies. Allow source-less
facades, preserve their own sources, and verify the complete A→U→C flow through
OOP MIR without ambiguous types.
```
