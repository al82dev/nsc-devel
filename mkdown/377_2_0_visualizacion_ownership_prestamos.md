# 377 — Visualización de ownership y préstamos / Ownership and borrow debugging

## Español

La información CodeView/PDB conserva ahora la cualificación semántica de parámetros y locales
`owned` y `borrowed`. El MIR retiene el ownership de la declaración y el backend publica vistas de
tipo nominales como `owned DebugExtreme.Counter*` y `borrowed DebugExtreme.ICounter*`, aunque el
ABI siga usando exactamente la misma representación física.

Las vistas de clases derivan, sólo en metadatos de depuración, del tipo físico original. Visual
Studio y WinDbg pueden distinguir al propietario del préstamo sin perder los campos del objeto ni
alterar el código generado. Las referencias sin layout concreto reciben una vista opaca estable.

La prueba O0 productor/consumidor inspecciona el PDB final, exige ambas vistas y comprueba los
símbolos locales, rangos de vida, fuentes y ejecución con salida 42. La construcción limpia se
instaló en `C:/native-sharp` antes de ejecutar la certificación dirigida.

## English

CodeView/PDB information now retains the semantic qualification of `owned` and `borrowed`
parameters and locals. MIR keeps declaration ownership and the backend publishes nominal type views
such as `owned DebugExtreme.Counter*` and `borrowed DebugExtreme.ICounter*`, while the ABI continues
to use exactly the same physical representation.

Class views derive, in debug metadata only, from the original physical type. Visual Studio and
WinDbg can distinguish an owner from a borrow without losing object fields or changing generated
code. References without a concrete layout receive a stable opaque view.

The O0 producer/consumer test inspects the final PDB, requires both views, and verifies local
symbols, live ranges, sources, and execution with exit code 42. The clean build was installed in
`C:/native-sharp` before running the focused certification.
