# Préstamo de propietarios compartidos de solo lectura

`Rc<T>.Borrow()` y `Arc<T>.Borrow()` crean una vista de solo lectura sin
modificar el propietario. Sus implementaciones internas toman el puntero del
bloque compartido y construyen `SharedBorrow<T>`. Los cuatro métodos públicos
e internos se declaran `readonly`, lo que permite usarlos desde un receptor
`in` y conserva la prohibición de invocar operaciones mutables sobre él.

La regresión `nsc.sdk.shared-readonly-borrow` compila las fuentes de las cuatro
familias en un proyecto temporal. Para Rc y Arc ejecuta una lectura de vista
prestada a un receptor `in` en O0 y O2 (retorno 42), y comprueba que `Clone()`
en ese mismo receptor sigue rechazado por ser potencialmente mutable.

Validación en A: la prueba CTest dirigida pasó y las cuatro ejecuciones
devolvieron 42. En una copia de las fuentes que conserva las cuatro
implementaciones anteriores a este cambio, la misma prueba falló porque el
receptor readonly no podía llamar `Borrow()`; evidencia en
`A:/nsc-fixes-evidencia/readonly-borrow-previous/previous.log`. El SDK aislado
actualizado permitió además verificar los consumidores `in` de la campaña.

Las pruebas usaron el compilador aislado de la rama con otros cambios locales
sin registrar; no constituyen validación de este commit con un compilador
construido desde un árbol limpio. La construcción del SDK intentó reutilizar
caché, pero no hubo acierto y procesó 71 fuentes. No se ejecutó la batería
completa ni se utilizó la instalación funcional.
