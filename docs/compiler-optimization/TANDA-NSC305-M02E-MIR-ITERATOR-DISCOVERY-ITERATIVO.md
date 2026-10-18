# Tanda NSC 3.0.5 M02E — descubrimiento iterativo de layouts iterator

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02E migra exclusivamente el recorrido `collectIteratorNode` de
`ProjectOopMirModel::importLayouts`. La hoja fue demostrada por el `0xC00000FD` del primer
contrafactual integrado M02D: la colección future ya había terminado de forma iterativa, pero
`importLayouts` entró después al DFS recursivo iterator con el mismo árbol de 32,768 niveles.

No cambia la detección de `iterator:<T>` ni `async-iterator:<T>`, la forma física de sus layouts,
campos, offsets, tamaños, alineaciones, error tipado, `MoveNext`, `Drop`, HIR, MIR, sintaxis, ABI,
NSMETA, diagnósticos ni LLVM. Los collectors tuple y future cerrados en M02A, M02B y M02D
permanecen intactos. No se fusiona este cambio con ningún recorrido MIR posterior.

## Algoritmo y paridad

La ruta histórica omitía nodos nulos; para cada nodo presente visitaba `typeKey`, `elementTypeKey`
y `argumentTypeKeys` en ese orden y después descendía por los hijos de izquierda a derecha. Las
claves se acumulaban con repeticiones. Sólo al terminar todos los cuerpos se aplicaban `sort` y
`unique`; después se publicaban los layouts.

`collectIteratorLayoutKeys` conserva esa frontera como helper privado productivo. Mantiene un
worklist LIFO de punteros HIR no propietarios, extrae un nodo, conserva el orden de sus tres familias
de claves y apila los hijos en reversa. Por ello el orden observable sigue siendo preorden
padre→hijos de izquierda a derecha. Los slots nulos se insertan y se descartan al extraer, igual que
el retorno temprano histórico. No se deduplica dentro del recorrido.

El helper future permanece independiente aunque ambos recorridos tengan forma parecida. M02E no
introduce una abstracción transversal ni modifica una hoja ya validada.

## Contrafactual profundo

El focus `mir-iterator-layout-depth` construye una cadena HIR de 32,768 nodos y un slot nulo. Coloca
claves duplicadas `async-iterator:builtin:long` en la raíz e `iterator:builtin:int` en la hoja más
profunda. Primero llama directamente el helper productivo y exige la secuencia cruda exacta, con
preorden y duplicados. Después mueve el mismo árbol a un modelo HIR y ejecuta `importLayouts`
completo; este paso reproduce la frontera que antes caía y comprueba:

- terminación sin recursión del stack del proceso;
- sort y deduplicación deterministas;
- publicación de exactamente dos layouts en orden canónico;
- cinco campos físicos por iterator sin error tipado;
- tipos exactos `builtin:long` y `builtin:int` en el campo item.

El selector y la llamada default viven en `nsc_project_oop_lowering_tests`. No se crea fuente,
ejecutable, target, caso de catálogo ni manifiesto. `MirLayoutImporter.cpp` sigue registrado una vez
en el CMake raíz y una vez en la compiler-suite; el target, la fuente de prueba y su `add_test`
siguen registrados exactamente una vez.

## Límites y validación

`collectIteratorLayoutKeys`, el wrapper de prueba y el contrafactual quedan por debajo de 60
statements ejecutables. La self-recursion `collectIteratorNode(collectIteratorNode, ...)` desaparece
sin modificar los algoritmos posteriores de layout.

Sobre HEAD `0dd1f6301d5da137dc6f27ade0df6c20d66c3be0`, después del cierre R01I, pasaron:

- build `nsc`, incluido el contrato de host estático;
- build `nsc_project_oop_lowering_tests`;
- focus `mir-iterator-layout-depth`: `PASS: 4 checks`;
- CTest `nsc.project-oop-lowering`: `1/1 PASS`;
- build `nsc_component_tests`;
- consumidor real síncrono `TL-LANG-381`: `PASS`;
- consumidor real asíncrono `TL-LANG-385`: `PASS`;
- validador canónico `validate_source_registration.py`: exit `0`.

La evidencia persistente está en:

- `A:\temp\m02e-build-nsc.log`;
- `A:\temp\m02e-build-lowering.log`;
- `A:\temp\m02e-focus-iterator-layout-depth.log`;
- `A:\temp\m02e-ctest-project-oop-lowering.log`;
- `A:\temp\m02e-build-component-tests.log`;
- `A:\temp\m02e-tl-lang-381.log`;
- `A:\temp\m02e-tl-lang-385.log`;
- `A:\temp\m02e-source-registration.log`.

No se ejecutó baseline ni suite completa.
