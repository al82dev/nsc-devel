# Copia implícita de referencias compartidas en tuplas

Una ubicación `Rc`, `WeakRc`, `Arc` o `WeakArc` podía copiarse directamente a
un elemento de tupla tanto con `(owner, 42)` como con `{ owner, 42 }`. Los
cuatro tipos son move-only; copiar sus bits no incrementa el contador y puede
crear dos propietarios del mismo bloque. Las ocho reproducciones públicas se
analizaron sin ejecutarse; sus resultados anteriores están en
`public-tuple-borrow-tuple_owner*` y `public-tuple-borrow-tuple_weak*` bajo
`A:/nsc-fixes-evidencia`.

El análisis de literal y el de inicializador con llaves rechazan una ubicación
move-only que se copiaría a un elemento de tupla. No cambian otros destinos.
Las dos formas con `move(owner)` ya eran rechazadas porque no se reconocían
como contextos consumidores; esta corrección no habilita esa operación
(`shared-tuple-move-probe/results.json`). Una ruta mediante condicional se
aborda separadamente en `conditional-move-only-copy.md`.

La regresión interna verificó el rechazo de ambas formas para los controles
local y atómico, y ejecutó clones y tuplas copiables en O0/O2. La regresión
pública verificó las cuatro familias y ejecutó clones fuertes y débiles en
ambas formas y niveles de optimización. Las trazas están en los logs
`readonly-array-ctest-tuple_owner_copy,tuple_owner_aggregate_copy,tuple_owner_clone,tuple_owner_aggregate_clone,tuple_ordinary,tuple_scalar.log`
y `public-tuple-borrow-tuple_owner_copy,tuple_owner_aggregate_copy,tuple_weak_copy,tuple_weak_aggregate_copy,tuple_owner_clone,tuple_owner_aggregate_clone,tuple_weak_clone,tuple_weak_aggregate_clone.log`.

El control existente `nsc.ns3.t02.native-tuples` esperaba un diagnóstico
posterior sobre drop flags para una deconstrucción. Ahora acepta también el
diagnóstico específico más temprano de copia move-only, sin dejar de exigir
rechazo. Los tres controles existentes de tuplas y agregados pasaron 3/3
(`tuple-owner-existing-controls.log`); el resultado anterior y su diagnóstico
se conservan en `tuple-owner-existing-before.log`.

Queda abierto un defecto distinto: si se construye un clon como primer elemento
y el segundo lanza por inicialización estática, se captura la excepción pero
el fuerte sigue retenido. Se observó con `Rc` y `Arc` en las dos sintaxis, O0;
la variante diagnóstica terminó con 4, que indica contador distinto de 1
(`public-tuple-borrow-tuple*exception_diagnostic-{Rc,Arc}.log`). La limpieza
de tuplas parcialmente inicializadas no queda corregida aquí. No se ejecutaron
las reproducciones que copiaban propietarios, y la API pública todavía no está
certificada para consumo binario.
