# Movimiento explícito en tuplas consumidas

El análisis rechazaba `return (42, move(input))` para las cuatro familias de
punteros compartidos aunque admitía `return move(input)`. El analizador de
tuplas procesaba cada elemento sin permiso de movimiento. Ahora habilita el
movimiento explícito del elemento cuando el contexto de la tupla consume el
resultado. Las comprobaciones existentes siguen rechazando copiar un valor
move-only, moverlo dos veces o usarlo después de moverlo.

Una primera variante habilitó también el movimiento dentro de una tupla
descartada y se observó que el temporal no destruía su propietario. La
restricción al contexto consumidor evita habilitar esa forma: la prueba
actual exige que se rechace, sin afirmar que el descarte esté implementado.
La salida inicial de ese ensayo fue sobrescrita; no se atribuye al log actual.

`nsc.sdk.shared-tuple-move-analysis` y `nsc.sdk.shared-tuple-move-binary`
pasaron 1/1 cada una con el compilador aislado y Core binario anterior. La
primera comprueba las cuatro familias y diagnósticos de propiedad; la segunda
ejecuta un productor y consumidor en O0/O2 y verifica sus contadores. Comandos
y resultados: `A:/nsc-fixes-evidencia/shared-tuple-move-commit-20261111/`.
Las pruebas se ejecutaron sobre un árbol con otros cambios locales; no prueban
una revisión limpia. Requieren `NSC_TEST_CORE_BINARY` y se omiten si falta.
Los controles de limpieza excepcional y otros cambios de cleanup quedan fuera
de este commit y no se consideran cerrados por él.

Los controles `nsc.sdk.shared-tuple-move-call` y
`nsc.sdk.shared-tuple-move-call-binary` verifican la entrega por valor a un
método del mismo proyecto y a uno de una biblioteca estática independiente.
Ambos cubren las cuatro familias, salida normal y excepcional, O0/O2; el
observador comprueba caducidad o contador al terminar la llamada. Pasaron 2/2
sin omisión (`A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/test.log`).
Usan Core binario anterior y un árbol con otros cambios locales. No acreditan
DLL, cruces de niveles diferentes ni una revisión limpia.

La reproducción dirigida `shared_tuple_move_partial_binary.py` construye una
biblioteca que mueve un propietario al primer elemento y lanza al construir el
segundo; un consumidor independiente comprueba el contador tras capturar el
error. Rc, WeakRc, Arc y WeakArc devolvieron el resultado esperado en O0 y O2
(8/8, `A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/partial.log`).
La prueba usa el compilador del árbol con cambios locales y el Core binario
anterior; no identifica cuáles cambios son necesarios, no acredita suspensión,
DLL ni una revisión limpia y aún no está registrada como prueba permanente.
