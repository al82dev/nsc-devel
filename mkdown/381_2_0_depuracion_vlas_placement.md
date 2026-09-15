# 381 — Depuración de VLAs y placement / VLA and placement debugging

## Español

HIR y MIR conservan ahora la forma de depuración que antes se perdía al convertir un VLA a su
representación interna de puntero. CodeView publica un arreglo de rango correcto con extensión
dinámica y mantiene cada dimensión capturada como un local legible, por ejemplo
`vlaValues.Length[0]` y `vlaValues.Length[1]`. El depurador puede así reconstruir dirección,
elemento y forma row-major usando los mismos snapshots que gobiernan el indexado real.

Una variable inicializada por `new (storage) T(...)` conserva también su procedencia. El PDB le da
una vista nominal `placement T*` que expone el objeto colocado sin afirmar que exista una
asignación de heap ni ownership del storage. Para structs la vista contiene el valor superpuesto;
para clases reutiliza el subobjeto de clase real.

El corpus O0 declara un VLA bidimensional, escribe y lee uno de sus elementos, construye un struct
en almacenamiento de pila y comprueba la ejecución. La inspección del PDB exige los dos tipos
`LF_ARRAY`, las dimensiones con nombres fuente, el local VLA y la vista `placement` del resultado.

## English

HIR and MIR now retain the debug shape that used to disappear when a VLA was converted to its
internal pointer representation. CodeView emits an array of the correct rank with dynamic extent
and preserves every captured dimension as a readable local, such as `vlaValues.Length[0]` and
`vlaValues.Length[1]`. The debugger can therefore reconstruct address, element, and row-major shape
from the same snapshots that drive real indexing.

A variable initialized by `new (storage) T(...)` also retains its origin. The PDB gives it a
nominal `placement T*` view that exposes the placed object without claiming a heap allocation or
ownership of the storage. For structs the view contains the overlapping value; for classes it
reuses the real class subobject.

The O0 corpus declares a two-dimensional VLA, writes and reads one element, constructs a struct in
stack storage, and verifies execution. PDB inspection requires both `LF_ARRAY` records, the
source-named dimensions, the VLA local, and the result's `placement` view.
