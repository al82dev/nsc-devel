# NSC 3.0.5 — H02S3: destrucción iterativa de roots del modelo HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Evitar agotamiento de pila al destruir los árboles poseídos por `ProjectOopHirModel`. |
| Algoritmo | Worklist de `unique_ptr`, reserva antes de transferir hijos y vaciado de cada nodo. |
| Símbolos | Destructor de `ProjectOopHirModel`, `destroyHirTreeIteratively` y uso de `static_assert`. |
| Incluido | Bodies de funciones, valores de field initializers y temporal de condición de `static_assert`. |
| Excluido | Cambiar lowering, formato HIR, aggregates, deconstruction, MIR o LLVM. |
| Terminación | Rojo/verde con dos roots de 32,768 niveles, regresiones static-assert/HIR y `nsc`. |

## Rojo reproducido

El foco `hir-model-destruction-depth` instala un body de función HIR de 32,768 niveles y deja que
el modelo salga de alcance sin extracción manual. Con el destructor predeterminado terminó con
`-1073741571` (`0xC00000FD`).

El fallo ocurre después de construir correctamente el árbol: la cadena de `unique_ptr` volvía a
usar la pila nativa durante el teardown. Por ello las pruebas profundas anteriores necesitaban
extraer el root y liberarlo manualmente; esa obligación no era una propiedad segura del modelo.

## Implementación

- `destroyHirTreeIteratively` vive ahora en el soporte HIR compartido y conserva ownership mediante
  una worklist de `unique_ptr`, sin liberar punteros crudos.
- Antes de mover los hijos se reserva el espacio requerido. Si la reserva falla, todavía no se ha
  desprendido ownership de esos hijos.
- Cada nodo vacía `children` después de transferirlos; su destrucción inmediata ya no encadena la
  destrucción de descendientes.
- El destructor de `ProjectOopHirModel` aplica el helper a todos los bodies de `functions_` y a
  todos los valores de `fieldInitializers_` antes de que se destruyan sus contenedores.
- La copia local del mismo algoritmo en `HirStatementMachine.cpp` se eliminó; `static_assert`
  consume ahora el helper único.
- El foco verde incluye simultáneamente un body y un field initializer de 32,768 niveles.

No se modificaron sintaxis, semántica, diagnósticos, formato HIR, API funcional, ABI ni NSMETA.

## Evidencia focal

- Body profundo con destructor predeterminado: rojo previo `0xC00000FD`.
- Body más field initializer, ambos a 32,768 niveles: `PASS 1/1`, exit `0`.
- `hir-static-assert-depth`: `PASS 2/2`.
- `hir-static-assert-short-circuit`: `PASS 2/2`.
- `hir-dump-depth`: `PASS 2/2`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.

H02 permanece abierto. Esta tanda cubre los roots poseídos por el modelo y el temporal explícito de
`static_assert`; las rutas recursivas de aggregates/deconstruction y cualquier temporal profundo
que no llegue al modelo todavía deben resolverse o clasificarse. No se ejecutó baseline, deploy,
instalación, batería completa ni consumidor externo.
