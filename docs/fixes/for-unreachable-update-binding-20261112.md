# Binding de actualización de for sin backedge

Sobre `e080f28a6`, un `for` con actualización `i = i + 1` y cuerpo que
termina incondicionalmente en `break` o `return` falló al generar el ejecutable
en O0/O2: faltaba el binding del identificador `i` en la actualización. Un
`for` con break condicional y un `while` de control ejecutaron 42. Comandos y
salidas: `A:/nsc-fixes-evidencia/for-break-binding-expanded-before-20261112.log`.
La reproducción sin vistas establece que no depende del SDK de punteros.

`ProjectMethodBodyModel::analyzeForUpdate` omitía la actualización cuando no
había backedge alcanzable, salvo para el `foreach` sintético, aunque HIR aún
necesita sus bindings. Ahora usa el mismo análisis temporal alcanzable para
cualquier actualización inalcanzable y restaura el flujo real terminado. La
actualización no se ejecuta ni pasa a ser alcanzable por esta comprobación.

La compilación incremental del compilador aislado pasó. Con nsc SHA-256
`9464c74e582ac4ca9f114c241615f7febb0247a56458b748681a2045604cb6a6`,
`tests/integration/for_unreachable_update.py` compiló y ejecutó seis casos en
O0/O2, incluyendo `break`, `return`, `continue`, break condicional y una
actualización con efecto observable que no debe ejecutarse. También rechazó
la lectura de una variable cuya única asignación está en la actualización
inalcanzable. Evidencia: `A:/nsc-fixes-evidencia/for-update-directed-final-20261112/`
y su `.log`.

El control de vistas con `for` y break, que había fallado en la tanda anterior,
compiló y ejecutó 42 en ambos niveles; los demás controles de vistas pasaron
(`view-loops-positive-runtime-forfix-20261112.log` y
`view-loops-after-for-update-20261112.log`). Dos controles dirigidos de
`foreach` con los handles local y atómico también pasaron en O0/O2; se usó un
adaptador de evidencia que fija el directorio de trabajo del proceso hijo a
la copia del repositorio (`foreach-after-for-fix-repo-cwd-20261112.log`). La
primera invocación de ese runner no recibió sus fuentes auxiliares y falló por
configuración; no se cuenta como prueba aprobada.

No se afirma cobertura universal de expresiones de actualización, efectos
excepcionales ni diagnósticos de código inalcanzable. No se ejecutó la batería
completa ni se modificó interoperabilidad C/C++.
