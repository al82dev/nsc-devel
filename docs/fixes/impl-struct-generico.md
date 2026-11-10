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
