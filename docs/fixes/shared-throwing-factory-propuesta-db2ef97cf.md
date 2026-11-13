# Construcción compartida con excepciones: propuesta pendiente de validar

## Validación posterior en la integradora

Con el compilador de `1bd04bae9`, SHA-256
`9dafe59b7ca23344db5ae5fa4cbbc107068fdf4d6e6ac9c482ca02857a04df87`, el
fixture de cuatro campos se compiló y ejecutó con resultado 42 en O0/O2,
usando Core binario de `integracion-vistas-sdk` y el runtime aislado. El
runner copió la fuente byte por byte y comprobó sus hashes y los de las
entradas antes y después. Comandos y resultados:
`A:/nsc-fixes-evidencia/shared-four-fields-current-sdk-1bd04bae9/`.
La barrera descrita abajo ya no se reproduce para este fixture y estos
artefactos; no se atribuye la corrección a un commit concreto.

La matriz dirigida de delegación se ejecutó en dos selecciones, sin repetir
casos: fallos en argumento, base, constructor terminal y cuerpo exterior,
además de éxito; construcción directa, fábrica Rc y fábrica Arc; O0/O2.
Las 30 ejecuciones compilaron y devolvieron 42. Los comandos, salidas y hashes
de compilador y Core se conservan en los `results.json` de
`A:/nsc-fixes-evidencia/shared-delegation-base-1bd04bae9/` y
`A:/nsc-fixes-evidencia/shared-delegation-selected-1bd04bae9/`. El primer
intento del lanzador seleccionado no encontró el módulo auxiliar Python; se
ajustó su ruta y se ejecutó de nuevo, antes de crear los resultados. El Core
binario usado se generó desde la copia integradora, según
`integracion-vistas-pruebas-20261112.md`; no demuestra compatibilidad con
otros productores ni prueba directamente la liberación física de cada bloque.

Una ejecución inicial usó por error el Core de `A:/nsc-fixes-evidencia/sdk`,
que no publica estas familias y falló al resolver sus nombres. Su resultado
está en `shared-four-fields-current-1bd04bae9/` y no constituye reproducción
del defecto de limpieza. No se modificaron esos artefactos. Persisten las
limitaciones de valores struct, temporales que lanzan después de delegación,
bases binarias y excepciones adicionales durante destrucción. No se ejecutó
batería completa ni pruebas de interoperabilidad C/C++.

Base de preparación: db2ef97cfb56a47a923beeeb0aa3a7df94c649f9. La propuesta y
los fixtures están integrados como trabajo en curso; no se han compilado ni
ejecutado después de la integración. El usuario indicó integrar el conjunto
antes de realizar la validación dirigida.

## Evidencia previa

La reproducción del worker se conserva en
`tests/integration/fixtures/shared_throwing_four_fields_original.ns`. Su texto
coincide con `A:/nsc-vistas-evidencia/shared-four-fields-failure/Main.ns` según
`fc`, pero sus bytes difieren por finales de línea; no es un ejemplo original
del reporte ni una copia binaria idéntica del archivo histórico.
Con un compilador aislado anterior, las configuraciones O0 y O2 lo rechazaron
con `shared throwing construction cannot safely clean this payload shape`.
Esa observación no valida este diff ni el Core binario de esta revisión.
El registro completo está en
`docs/fixes/shared-throwing-four-fields-integrador-20261112.md`.
Una ejecución anterior de un control de delegación Arc excedió 30 segundos;
su causa no está identificada y no se interpreta como prueba de doble drop.

## Propuesta

Reutilizar la comprobación estructural existente del estado vacío de las cuatro
familias. Durante una delegación, los flags de campos y base permanecen
inactivos hasta el retorno normal de la llamada; la activación precede al
cleanup de la expresión completa. El guard recorre constructores this/base
y marcadores de inicialización de bases implícitas; exige cuerpos MIR de
origen y entradas locales de destrucción. Las bases binarias no triviales y
los valores y layouts no demostrados siguen rechazados por la barrera.
La acción sintética se incorpora a la limpieza de un throw explícito.

## Validación pendiente

La matriz `tests/integration/shared_throwing_delegation_base.py` genera
quince fuentes para construcción directa, MakeRcThrowing y MakeArcThrowing:
fallo de argumento, base, constructor terminal y cuerpo exterior, y éxito.
Usa las cuatro familias en campos, y los comandos de O0 y O2 verifican
captura, orden y conteos. El runner recibe rutas explícitas de compilador,
Core y metadata aislados en A y una carpeta nueva en A; `--execute` activa
compilación y ejecución. No se ejecutó esta matriz con la propuesta.

Faltan además el original con Core de la revisión, structs, bases implícitas,
movimiento, reemplazo fallido, fallo tras retorno por destructor de temporal,
propiedad y liberación física del bloque, metadatos binarios y efectos de un
destructor de base que lanza durante otra excepción. La acción de base usa el
mismo mecanismo de llamada sin destino local de unwind que la acción anterior;
no demuestra que el resto de limpiezas continúe si ese destructor lanza.
La identidad del estado vacío entre particiones y la interacción con SIMD
necesitan pruebas. El commit base contiene trabajo SIMD en progreso.

El runner `shared_throwing_four_fields_original.py` copia byte a byte el
fixture versionado y recibe nsc, Core, metadata, runtime, imports y directorio
de evidencia explícitos en A. El runner se incorporó sin ejecutarse; su copia
idéntica se refiere al fixture versionado, no al archivo histórico en evidencia.
