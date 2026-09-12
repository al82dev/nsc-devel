# 244 — Delete atómico de elementos poseídos / Atomic deletion of owned elements

**Línea / Line:** `1.0.1-dev`

## Resumen

`delete items[i]` sobre un elemento referencia ahora opera sobre la ranura y no sobre una copia
aislada del puntero: calcula una sola vez la dirección, carga el valor, escribe null y después
ejecuta la destrucción dinámica o libera el recurso nativo. Así, el almacenamiento deja de
conservar un owner colgante después de liberar su objeto.

## Defecto

La entrega 225 hizo correcta la sustitución de una ranura poseída: antes de almacenar el nuevo
valor libera el anterior. Esa regla reveló que `delete items[i]` tenía una semántica incompleta.
Liberaba el objeto pero no vaciaba el elemento. Una escritura posterior veía el mismo puntero aún
presente, aplicaba release-before-store y destruía por segunda vez la asignación ya liberada.

El defecto afecta arreglos nativos, arreglos fijos y elementos de raw pointer cuando contienen
referencias o strings. Los elementos valor continúan evaluándose por sus efectos y `delete` sigue
siendo un no-op para ellos. Los indexadores de usuario no se convierten en direcciones ni cambian
de contrato.

## Implementación

- `ProjectOopHirModel` reconoce exclusivamente un `ArrayElement` o `RawElement` directo y lo baja
  en contexto de dirección cuando su elemento requiere destrucción.
- El nodo `Deallocation` conserva el tipo y una marca de limpieza atómica; HIR a MIR ya transporta
  ambos campos sin una estructura paralela.
- `ProjectOopLlvmLowerer` carga el valor desde la dirección y pone la ranura a null antes de llamar
  al destructor. El orden previo a la llamada también es seguro si la destrucción propaga una
  excepción.
- La ruta de `delete[]`, la prohibición de `delete` sobre campos `owned`, y la destrucción de una
  referencia no direccionable permanecen sin cambios.

## Evidencia dirigida O0

La prueba crea una ranura `owned Resource[]`, instala un recurso, lo elimina, reutiliza la misma
ranura y finalmente la limpia. El resultado debe ser exactamente dos destrucciones. Sin la
corrección, la segunda asignación intenta liberar de nuevo el primer puntero. La evidencia se
ejecuta sólo en O0 y no dispara un baseline.

## Archivos

- `src/Frontend/ProjectOopHirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/Stage50_11OwnershipExecutionTests.cpp`
- `mkdown/244_1_0_1_delete_elemento_poseido_atomico.md`

---

## English

`delete items[i]` on a reference element now acts on the slot rather than on an isolated pointer
copy. The compiler evaluates the address once, loads the value, clears the slot, and only then
performs dynamic destruction or native deallocation. This prevents a later release-before-store
operation from destroying a dangling owner twice.

HIR recognizes only directly addressable native/fixed-array or raw-pointer elements. Value
elements retain their evaluated no-op deletion behavior, and semantic indexers are not treated as
addresses. LLVM clears the slot before invoking user destruction so exceptional cleanup also sees
an empty owner. The directed O0 execution replaces a deleted slot and observes exactly two
destructions. No baseline is run for this delivery.

## Commit de referencia / Reference commit

**Título ES:** Hace atómico delete sobre elementos poseídos

**Title EN:** Make deletion of owned elements atomic

**Descripción ES:** Vacía una ranura referencia antes de destruir su valor para impedir dobles
liberaciones al reutilizar almacenamiento poseído.

**Description EN:** Clear a reference slot before destroying its value to prevent double releases
when owned storage is reused.
