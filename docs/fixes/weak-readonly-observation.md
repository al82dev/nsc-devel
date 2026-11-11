# Consultas de referencias débiles desde `in`

`WeakRc<T>.Expired/WeakCount` y `WeakArc<T>.Expired/WeakCount` solo consultan el
handle y sus contadores, pero no estaban declarados `readonly`. En el Core
binario previo una función con `in WeakArc<Payload>` era rechazada al invocar
ambas consultas (`A:/nsc-fixes-evidencia/weak-readonly-observation/check.log`).
La reproducción desde fuentes llegó al mismo rechazo para WeakRc; el runner se
detuvo en esa primera familia (`before.log`).

Las dos consultas se declararon `readonly` en cada envoltura pública y en sus
handles internos. No se cambian clonación, promoción ni el contrato del conteo
atómico: durante operaciones concurrentes sigue siendo una observación
aproximada, y sin ellas es exacto.

La selección dirigida desde fuentes probó ambos tipos débiles vacíos, vivos y
expirados, además de clonación y movimiento de débiles expirados. Las 12
ejecuciones de Rc/Arc en O0/O2 devolvieron 42 (`controls.cmd` y `controls.log`
en la carpeta de evidencia). La prueba independiente
`tests/integration/shared_weak_readonly_binary.py` produjo una biblioteca
estática mínima con las fuentes relevantes de Core y compiló consumidores
separados en O0/O2: las cuatro ejecuciones devolvieron 42 (`binary.cmd` y
`binary.log`). El productor se compiló en O0, incluyó contratos mínimos de
excepción y traits y no es el SDK completo. El compilador contiene otros
cambios locales; la prueba no acredita DLL ni una instalación limpia.

El Core binario completo preexistente no se regeneró en esta comprobación para
evitar reconstruir todo el SDK. La regresión binaria está registrada como
`nsc.sdk.shared-weak-readonly-binary`; su productor pequeño se genera en un
temporal aislado con caché de artefactos propia. Ninguna prueba establece una
garantía de instantánea exacta de WeakCount durante concurrencia.

La regeneración de la configuración aislada y el descubrimiento dirigido
`ctest -N` encontraron exactamente esa entrada (`configure.log` y
`registration.log`). Esta comprobación no ejecutó la prueba ni reconstruyó
Core. La evidencia funcional sigue correspondiendo a las ejecuciones y al
compilador identificados en `controls.log` y `binary.log`; no acredita una
revisión limpia del compilador ni del SDK completo.

La primera forma de `Observe` omitía `WeakCount()` mediante cortocircuito
cuando el débil estaba vacío o expirado. Se corrigió el caso desde fuentes
para ejecutar ambas consultas mediante `in` en los tres estados. La selección
repetida pasó las 12 ejecuciones de las dos familias y los dos controles
adicionales en O0/O2 (`controls-complete.cmd` y `controls-complete.log`). La
prueba binaria anterior conserva esa limitación: sólo consulta `WeakCount()`
mediante `in` cuando el débil está vivo; los otros estados se comprobaron con
acceso ordinario o únicamente `Expired()`.

La regresión binaria se amplió después para consultar ambos métodos mediante
`in` en los tres estados. La entrada dirigida de CTest pasó y sus cuatro
consumidores Rc/Arc en O0/O2 devolvieron 42 (`binary-complete.cmd` y
`binary-complete.log`). El productor de prueba compiló 18 fuentes de Core y un
archivo de contratos mínimos en una carpeta temporal nueva; no utilizó un
SDK completo y no fue una recompilación incremental de ese productor. No se
reconstruyeron el compilador ni el SDK completo. Permanecen sin demostrar la
distribución con Core completo, DLL y el resto del contrato de los punteros.

La comprobación sintáctica `python -B -m ast` de ambos ejecutores terminó con
código 0 (`static-check.cmd`, `binary-python-ast.log` y
`source-python-ast.log`). Las compilaciones de las regresiones comprueban la
semántica de las fuentes Core utilizadas; no se ha demostrado un análisis
estático independiente completo para el SDK ni sus consumidores.
