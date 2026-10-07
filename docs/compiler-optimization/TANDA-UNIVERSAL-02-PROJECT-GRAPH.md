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

Después del commit se reconstruirá desde cero y se ejecutará la batería completa O0 con NSLink.
