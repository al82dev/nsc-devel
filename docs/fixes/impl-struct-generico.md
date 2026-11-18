# Impl externo sobre una estructura genérica

La variante genérica de la decisión del caso 68 se rechazaba aunque el tipo y
el `impl<T>` pertenecieran al mismo módulo y namespace. La reproducción mínima
de `tests/integration/generic_struct_trait_impl.py`, con `Wrapper<T>` y
`HasValue`, recibió el diagnóstico de coherencia de tipo no declarado; el
control equivalente con clase sí pasó. Evidencia anterior al cambio:
`A:/nsc-fixes-evidencia/generic-struct-trait-before.log`.

La recolección de `ProjectGenericModel::collectTraitImplementationNodes` solo
agregaba clases genéricas a la lista de destinos, aunque la representación del
`impl` también permite estructuras. El cambio agrega declaraciones de struct
genéricas a esa lista; conserva las comprobaciones existentes de módulo,
namespace, forma genérica, condiciones y duplicados. No modifica el contrato de
`unsafe impl` ni el análisis `Send`/`Sync`.

El compilador se reconstruyó incrementalmente en
`A:/nsc-fixes-evidencia/build`; la salida está en
`A:/nsc-fixes-evidencia/generic-struct-trait-build.log`. La regresión dirigida
compiló y ejecutó con salida 42 tanto la estructura como el control de clase
en O0 y O2. Rechazó impl huérfano, duplicado y condiciones distintas; una
biblioteca con la estructura y su impl se consumió y ejecutó en O0 y O2 con
salida 42. Registro: `generic-struct-trait-extended.log` en la misma carpeta.
La prueba existente `nsc.compiler.struct-trait-impl` pasó 1/1 y la selección
`dependent_generic_constraints.py` pasó, incluidos consumidores binarios;
registros `generic-struct-trait-existing.log` y
`generic-struct-trait-dependent.log`.

La regresión nueva se ejecuta directamente con el compilador de la copia y
temporales aislados; todavía no está registrada en CTest. El compilador usado
proviene del build incremental con cambios concurrentes sin registrar en el
árbol de trabajo, por lo que esto no certifica un árbol limpio. No se han
demostrado todas las combinaciones de traits o ámbitos. Este cambio es un
prerrequisito para estudiar el contrato de `Arc` y no habilita su transferencia
entre hilos. No se ejecutó una batería completa.

## Restricciones coincidentes y consumo binario: reproducción pendiente

Una ampliación posterior usa `where T : Bound` tanto en el tipo como en su
`impl`. Desde fuentes, las variantes struct y clase ejecutaron 42 en O0 y O2;
registro `generic-struct-trait-constrained.log`. Al publicar la estructura
en una biblioteca, el productor terminó correctamente, pero el consumidor
rechazó la extensión en O0 y O2 con el diagnóstico de condiciones distintas:
`generic-struct-trait-constrained-binary.log`.

La reproducción persistida en `generic-struct-binary-condition/` muestra en
la metadata `where T : TraitProducer::Bound` en la plantilla y
`where T : Bound` en el impl. La comparación actual
`genericConstraintShape` compara las escrituras y encuentra una diferencia;
`published-constraints.log` conserva ambos fragmentos. No basta eliminar
calificadores por texto: podrían existir nombres iguales de tipos diferentes.
Un control separado con clase genérica produce el mismo rechazo al consumir
la biblioteca (`generic-class-binary-condition/build.log` y `check.log`), de
modo que no se atribuye este comportamiento exclusivamente al cambio de
estructuras. Todos los registros de este apartado están bajo
`A:/nsc-fixes-evidencia`.

La regresión ampliada permanece en el árbol y termina con código 1 por este
fallo binario; no se ha cambiado el productor de metadata ni la comparación.
La solución debe establecer la identidad real de las restricciones sin
confundir homónimos y sin modificar interoperabilidad C/C++. La comprobación
con `unsafe impl` condicional y la transferencia de Arc siguen pendientes.

## Corrección de la publicación de restricciones

`discoverTemplateDependencies` ahora recoge las referencias de tipos de las
restricciones del `impl` genérico en el ámbito léxico del propio `impl` y las
agrega al registro de su plantilla propietaria, después de confirmar que la
extensión fue asociada a esa plantilla. La publicación existente recibe así
las referencias necesarias para calificar ambas restricciones. No se modificó
el código de publicación de metadata ni se equiparan nombres por su escritura.

Después de reconstruir incrementalmente `nsc`, la regresión ampliada pasó en
O0/O2 desde fuentes y desde biblioteca para estructuras y clases con
restricciones coincidentes. En el productor binario el tipo y el `impl` están
en archivos diferentes y usan un namespace importado. También pasaron los
controles de tipo e impl sin restricciones; se rechazaron el impl huérfano,
duplicado, con condiciones distintas y con un tipo homónimo de otro namespace.
Registro: `A:/nsc-fixes-evidencia/generic-impl-constraint-final.log`.

Pasaron además la selección dirigida `dependent_generic_constraints.py`,
`nsc.compiler.struct-trait-impl` y la prueba
`nsc.stage48-2.external-template-type-qualification`; sus registros tienen el
prefijo `generic-impl-constraint-` en la misma carpeta. La última prueba se
recompiló incrementalmente antes de ejecutarse. La regresión nueva sigue siendo
un script dirigido sin registro CTest. El build y SDK proceden de un árbol con
otros cambios concurrentes, no de una revisión limpia. Estos controles no
demuestran todos los tipos de restricciones, ámbitos ni ciclos de caché, y no
implementan el contrato condicional de transferencia de Arc.
