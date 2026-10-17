# NSC 3.0.5 — R04C1: secciones `switch` iterativas

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `ab4418e2a`.
- **Hipótesis causal única:** cada statement de una sección `switch` se consume mediante
  `parseStatement`; un `switch` hijo conserva así un frame C++ del padre hasta cerrar todas sus
  secciones.
- **Entrada exacta:** statements `switch` anidados dentro de secciones `case/default`.
- **Invariante:** la profundidad de `switch` statements debe residir en la máquina heap-backed de
  controles estructurados, sin modificar la gramática o el AST.
- **Archivos de producto autorizados:** `src/Parser/Parser.cpp`; no se amplía la API pública.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y el
  tracking externo.
- **Contratos congelados:** condición ordinaria o declarativa, labels y su orden, agrupación de
  labels, `integerValue`, no-fall-through, flatten de `conditional-pattern-inline`, ranges,
  recovery/progreso, orden/texto de diagnostics, sintaxis, semántica, API, ABI, NSMETA y CLI 3.0.4.
- **Prohibido:** modificar pattern parsing, `match`, loop/comprehension, cleanup/exceptions,
  aumentar stack, imponer límite de profundidad, baseline, suite completa, deploy o instalación.
- **Gate:** conservar las 91 comprobaciones R04B; agregar profundidad parser directa y consumo
  method-bodies para `switch`; construir `nsc`; ejecutar sólo pruebas parser focales; auditar y
  crear un commit local aislado.
- **Terminación:** R04C1 sólo cierra cuando header, labels, sections, statements y finalización del
  `switch` se conducen por continuaciones explícitas. R04C sigue abierto para match y la
  clasificación precisa de loop/comprehension.

## Diseño autorizado

La máquina existente obtiene cuatro tareas nuevas:

1. `ParseSwitch` consume header y crea el nodo padre.
2. `ContinueSwitch` valida el comienzo de sección y consume sus labels síncronamente.
3. `ContinueSwitchSection` consume siblings; un control estructurado hijo se agenda en la misma
   pila en lugar de reentrar al stack C++.
4. `AppendSwitchStatement` conserva el flatten especial de condition-pattern y agrega el resultado
   a la sección activa.

## Implementación

- `switch` se agregó al reconocimiento de controles estructurados de la máquina compartida.
- El header conserva la bifurcación entre expresión gobernante y condition declaration.
- Cada grupo de labels se construye antes de recorrer los statements de su sección.
- `ContinueSwitchSection` agenda controles o bloques hijos sobre la pila explícita y sólo reanuda
  la sección cuando el hijo está materializado.
- `conditional-pattern-inline` conserva su flatten histórico; el resto de statements se anexa sin
  reinterpretación.
- El wrapper `parseSwitchStatement` ya no recorre cuerpos ni se autorreentra: delega a la máquina.

## Validación focal

- `nsc_project_method_body_model_tests`: **PASS 92/92**.
  - conserva las 91 comprobaciones R04B;
  - conserva 256 `switch` anidados consumidos por method-bodies;
  - agrega 4,096 `switch` anidados parseados directamente.
- `nsc`, `nsc_parser_tests` y el foco method-bodies construyen con `/W4 /WX`.
- Contrato estático de host runtime de `nsc.exe`: PASS.
- `nsc.parser`: 22/23; permanece únicamente la expectativa ajena del ternario de NativeSharp
  1.0.1, cuya entrada no contiene `switch` ni alcanza esta ruta. No se contó como PASS.
- `git diff --check`: PASS.

No se ejecutaron baseline, suite completa, deploy, instalación, push, pull ni fetch.

## Cierre

R04C1 queda terminado. R04C continúa abierto para R04C2 (`match`) y R04C3
(loop/comprehension y su frontera con R03).
