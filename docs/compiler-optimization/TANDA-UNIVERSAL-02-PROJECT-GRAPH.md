# Tanda universal 02: grafo de proyectos sin edición

## Objetivo

Eliminar la excepción funcional que permitía ciclos de referencias únicamente cuando todos los proyectos declaraban una edición concreta.

## Cambios

- El recorrido del grafo ya no transporta una pila de ediciones.
- Todo ciclo de `ProjectReference` se rechaza con la misma regla estructural y el mismo diagnóstico determinista.
- La prueba histórica de módulos semánticos conserva su nombre, pero crea proyectos sin `Language` y comprueba el contrato universal tanto en el cargador como en el driver.
- Los ciclos de tipos por valor no ocultan el error anterior del grafo: primero se rechaza el ciclo de proyectos.
- Las pruebas del grafo sin edición y la prueba histórica dejan de tener expectativas contradictorias.

## Archivos

- `src/Compilation/ProjectGraphLoader.cpp`
- `tests/unit/NativeSharp3T12SemanticModuleTests.cpp`

## Verificación

Ejecutada después del commit `466112307` mediante reconstrucción limpia y batería completa O0 con NSLink.

- Corrida: `universal-o0-t02-20261007`.
- Evidencia: `A:\\nsc-devel\\_nscbuilder\\history\\universal-o0-t02-20261007\\report.md`.
- Construcción del compilador: correcta.
- Construcción de los 429 ejecutables CTest: correcta (`1151/1151` pasos).
- CTest completo: `639/684` aprobados; `45` fallos restantes.
- `nsc.ns3.t12.semantic-modules`: aprobado.
- `nsc.project-graph`: aprobado.
- Ambos fallos del alcance de la tanda desaparecieron; el total bajó de `48` a `45`.
- `nsc.compilation-artifact-cache` también pasó en esta corrida, pero no se atribuye a este cambio porque era un fallo concurrente intermitente y el código de caché no fue modificado.
- Fixtures O0: `89/90` aprobados; permanece `cc1_c23_views_and_lifetimes` con salida `8` en vez de `42`.
- Bug triage O0: `9/9` aprobado.
- No se modificó `C:\\native-sharp` y no se certificó un baseline.

La tanda queda implementada y verificada en su alcance. La batería integral todavía conserva fallos pertenecientes a las siguientes tandas universales.
