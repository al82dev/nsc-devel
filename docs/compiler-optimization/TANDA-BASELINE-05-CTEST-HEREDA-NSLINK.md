# Tanda baseline 05 — CTest hereda NSLink sin anulaciones ocultas

## Objetivo

Cerrar una fuga en la política introducida por la tanda 04. NSCBuilder ya seleccionaba NSLink
como enlazador primario mediante `NSC_LLD_LINK`, pero cuatro pruebas de depuración reemplazaban
esa variable dentro de su propio comando CTest y ejecutaban siempre `lld-link`.

Después de esta tanda, esas pruebas respetan el enlazador elegido por NSCBuilder. En el modo
predeterminado prueban NSLink; durante un diagnóstico selectivo vuelven a ejecutarse con LLD sólo
si habían fallado con NSLink.

## Diagnóstico de la corrida cancelada

La corrida `20261001-164857-3d5ae4c0` alcanzó 567 de 569 pruebas antes de ser cancelada. En esa
evidencia no apareció un error de enlace atribuible a NSLink:

- las pruebas ordinarias de emisión de ejecutables pasaron;
- Stage 46 native libraries pasó;
- las fallas observadas pertenecían a self-host, contratos o diagnósticos previos al enlace;
- el fallback selectivo con LLD no alcanzó a comenzar porque la pasada CTest primaria no terminó.

La auditoría del `CTestTestfile.cmake` generado encontró cuatro anulaciones reales de la selección
del builder:

- `nsc.debug.codeview-pdb`;
- `nsc.debug.extreme-o0`;
- `nsc.debug.inline-stepping-o0`;
- `nsc.debug.assembly-stepping-o0`.

## Cambio aplicado

Los cuatro registros CTest dejaron de envolver el verificador con `cmake -E env` y dejaron de
asignar `NSC_LLD_LINK` a LLD. Ahora el proceso hereda el valor establecido por NSCBuilder.

No se eliminó `NSC_DEBUG_LLD_LINK_EXECUTABLE`: otras pruebas lo reciben como una herramienta
explícita para inspecciones o comparaciones que forman parte de su propio contrato. Tampoco se
alteraron las pruebas self-host que comparan deliberadamente artefactos producidos por ambos
enlazadores. Esas referencias a LLD no sustituyen la ruta primaria seleccionada por NSCBuilder.

## Archivos modificados

- `CMakeLists.txt`
  - elimina las cuatro anulaciones locales de `NSC_LLD_LINK`;
  - conserva los verificadores y todos sus argumentos de PDB/CodeView.
- `tests/selfhost/test_bootstrap_infrastructure.py`
  - agrega una regresión que exige que las cuatro pruebas sigan registradas;
  - prohíbe volver a fijar `NSC_LLD_LINK` a LLD dentro de `CMakeLists.txt`.

## Archivo nuevo

- Este documento. Registra el hallazgo, el alcance exacto, los archivos afectados, la mejora y
  las validaciones de la tanda.

## Mejora

El modo normal del baseline ya no mezcla silenciosamente NSLink y LLD en esas cuatro pruebas. La
evidencia del canal NSLink corresponde al enlazador primario seleccionado y el fallback LLD queda
reservado para fallos exactos, como establece la política de la tanda 04.

## Validación específica

- Regresión de infraestructura NSCBuilder: 1/1 aprobada.
- Regeneración del árbol `build-test-nslink`: correcta, sin recompilar ejecutables.
- Auditoría del CTest generado: 0 asignaciones forzadas de `NSC_LLD_LINK=lld-link`.
- Pruebas ejecutadas explícitamente con el NSLink de `A:\native-sharp-3.0-baseline`:
  4/4 aprobadas en 2.62 s.
- Prueba individual previa de CodeView/PDB con NSLink: aprobada.
- Baseline completo: no ejecutado; la corrida del usuario estaba cancelada.
- Instalación o modificación de `C:\native-sharp`: ninguna.
- `_abi/gates/G00/gate.json`: cambio preexistente del usuario, preservado y excluido del commit.
