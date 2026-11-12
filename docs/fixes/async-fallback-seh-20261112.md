# Reproducción y análisis del fallback SEH

La revisión de trabajo es `development-fixes-limitaciones`; este registro no
certifica un build limpio. Se usó el compilador aislado
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`, que informó el build hash
`b2a62146822e8e54`, y el runtime y bibliotecas de importación configurados en
`A:/nsc-fixes-evidencia/build`. Los temporales, cachés de usuario y resultados
se dirigieron a `A:/nsc-fixes-evidencia`.

Desde `A:/nsc-devel-fixes`, el comando
`A:/nsc-fixes-evidencia/async-finally-20261111/replacement-no-version-20261112.cmd`
ejecutó `tests/integration/async_finally_exception_replacement.py`; el runner
compartido invocó compilador y ejecutables con cwd del repositorio. El proyecto
temporal no seleccionó una versión de lenguaje. Se mantuvieron los fixtures
originales. El resultado completo está en el `.log` de igual nombre:

* `sync_preserve`, `sync_nested_preserve`, `sync_replace` y
  `sync_call_replace`: salida 42 en O0 y O2.
* `sync_inner_catch`: fallo LLVM `Cleanup funclets for the SEH personality
  cannot contain exceptional actions` en O0 y O2.
* `async_ready_replace` y `async_suspended_replace`: fallo de lowering
  `semantic invariant forbids structured exit across an active cleanup
  funclet` en O0 y O2.

El comando `A:/nsc-fixes-evidencia/async-finally-20261111/catch-owner-recheck-20261112.cmd`
ejecutó el control compartido `async_catch_owner_continuation.py`: sus diez
casos devolvieron 42 en O0 y O2. El log correspondiente documenta los veinte
resultados. Estas pruebas no cubren la propiedad ante una excepción extranjera
ni resuelven los fallos de reemplazo.

## Causa acotada y límites

En `OopLlvmImplementation::emitExceptionalCleanupChain`, el cuerpo de cleanup
se emite dentro del `cleanuppad` nativo; el catch filtrado de NativeSharp y el
fallback para excepciones extranjeras son rutas distintas. El documento
`agente3-async-evidencia.md` registra el ensayo que omitió el fallback: el
caso síncrono pasó, pero esa omisión elimina una limpieza necesaria y no es una
corrección. La rutina de LLVM
[`calculateSEHStateNumbers`](https://llvm.org/doxygen/WinEHPrepare_8cpp_source.html)
rechaza una acción excepcional anidada que usa el `cleanuppad` como padre;
ese código de referencia público no acredita por sí solo la revisión del LLVM
utilizado para esta compilación. La
[documentación de excepciones de LLVM](https://llvm.org/docs/ExceptionHandling.html)
describe los funclets y la diferencia con un modelo general de relanzamiento.

El ensayo nativo previo `eh-replace-investigation/finally-outline-nested.ll`
demostró para un ejemplo aislado que un helper invocado desde un cleanup puede
capturar internamente otra excepción y permitir que la excepción externa
continúe; no demuestra el lowering de NativeSharp ni las salidas suspendibles.
El registro `shared-async-drop.md` conserva un ensayo distinto donde una
excepción que escapa del helper mostró diferencias con optimización al inlinear
el llamador. No se infiere de ello una regla general de `noinline`.

Para un candidato síncrono acotado al caso real se deben transferir al helper
las direcciones efectivamente utilizadas, incluido el slot del catch interno.
El lowering también debe aislar y restaurar las estructuras de emisión que
pueden contener bloques o valores LLVM de la función anterior, en particular
`exceptionDispatchBlocks_`, `cleanupChainCache_`, `cleanupRegions_`,
`localStorage_`, handlers, funclets y estado de depuración. Hay que justificar
el manejo de parámetros, flags de destrucción, capturas y receptor para todo
cuerpo admitido por la guarda del candidato. La excepción interna puede no ser
capturada y escapar del helper; una solución debe conservar también esa ruta y
su comportamiento en O0/O2. El problema async de reanudación y dominancia
requiere un tratamiento separado: no se ha demostrado que el helper síncrono
lo resuelva.

**Estado:** tres reproducciones abiertas; sin parche funcional integrado.
Siguiente acción: definir y comprobar una guarda MIR y un protocolo de estado
que cubran los cuerpos admitidos, seguido de controles de excepción extranjera,
propagación, locales y optimización antes de integrar código. No suprimir el
fallback ni convertir el experimento de LLVM aislado en prueba del compilador.
