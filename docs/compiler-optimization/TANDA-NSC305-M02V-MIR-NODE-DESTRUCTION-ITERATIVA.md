# NSC 3.0.5 — M02V: destrucción iterativa del árbol MIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Evitar recursión de proceso al liberar árboles `ProjectOopMirNode`. |
| Algoritmo | Postorden iterativo con frames propietarios `(node,nextChild)`. |
| Símbolos | `ProjectOopMirNode::~ProjectOopMirNode` y helper local de ownership. |
| Incluido | Hijos, roots directos, cuerpos de función, inicializadores y reemplazos de caché. |
| Excluido | Layout del nodo, lowering, recorridos de lectura, formato de caché, API del lenguaje, ABI y NSMETA. |

## Ownership auditado

Todo ownership productivo de nodos MIR converge en tres `unique_ptr` declarados en
`ProjectOopMirModel.h`: `ProjectOopMirNode::children`, `ProjectOopMirFunction::body` y
`ProjectOopMirFieldInitializer::value`. Eso incluye los resultados y temporales de
`MirNodeMachine`, los vectores `functions_`/`fieldInitializers_`, el rollback de restore, los
`reset` de marcador nulo y el reemplazo transaccional `out = std::move(root)`. No existe otro
owner `shared_ptr`, raw owning pointer ni movimiento por valor de `ProjectOopMirNode`.

Por ello una sola corrección central en el destructor cubre todos los puntos sin cambiar el modelo
propietario. La fuente nueva se registra tanto en `nsc_frontend` como en la réplica de
compiler-suite.

## Algoritmo y contrato

El destructor mantiene una pila explícita de pares `(unique_ptr,node-child-index)`. Desciende por
los hijos de izquierda a derecha; sólo libera un nodo después de mover y liberar todos sus hijos.
Antes de soltar cada `unique_ptr` vacía `children`, por lo que la reentrada inevitable al destructor
es superficial y nunca proporcional a la altura. Los slots nulos se consumen sin crear frames.

Se conserva así la relación children-before-parent, el orden entre hermanos, el lifetime de cada
campo y los tipos propietarios existentes. `ProjectOopMirNode` no cambia de tamaño ni obtiene
estado auxiliar. El destructor es `noexcept`, como el destructor implícito anterior. La worklist
puede reservar memoria; un fallo de asignación conserva la terminación exigida por `noexcept`,
pero desaparece el stack overflow determinista de árboles válidos profundos.

## Contrafactual e infraestructura

El focus `NSC_PROJECT_OOP_LOWERING_FOCUS=mir-node-destruction-depth` crea y libera, de forma
secuencial, tres árboles de 32,768 niveles: root directo, `ProjectOopMirFunction::body` y
`ProjectOopMirFieldInitializer::value`. Cada árbol contiene además el slot nulo ya producido por
`makeDeepMirBlockTree`. Las tres operaciones usan `reset()` directamente y no el helper iterativo
de pruebas, por lo que ejercitan el destructor productivo.

No se crea ejecutable ni archivo de prueba nuevo. El caso vive en
`nsc_project_oop_lowering_tests`, registrado como `nsc.project-oop-lowering`; no aplican catálogo,
manifest ni Builder externos. La fuente productiva nueva sí exige doble registro CMake y
`validate_source_registration.py`.

## Estado de validación

Implementación, auditoría estática y gates focales cerrados:

- `nsc`, `nsc_project_oop_lowering_tests` y `nsc_ns3_p08_p10_explain_tests` construidos; contrato
  estático host/runtime PASS (`A:\temp\m02v-build-targets-rerun.log`).
- focus `mir-node-destruction-depth` PASS 3/3 sobre root, body e initializer de 32,768 niveles
  (`A:\temp\m02v-focus-node-destruction.log`).
- CTest `nsc.project-oop-lowering` y `nsc.ns3.p08-p10.explain` PASS 2/2
  (`A:\temp\m02v-ctest-lowering-p08p10.log`).
- `validate_source_registration.py` PASS (`A:\temp\m02v-source-registration.log`).

La línea de la réplica compiler-suite quedó integrada en el HEAD inmediatamente anterior junto al
hunk CMake concurrente de R03C1; este cierre conserva esa línea y aporta el registro raíz, fuente,
declaración, prueba y expediente restantes. No se ejecutó baseline, deploy, instalación, batería
completa ni los smokes TL opcionales, porque el contrato ejercitado es lifetime del host C++ y los
dos targets focales lo cubren directamente.
