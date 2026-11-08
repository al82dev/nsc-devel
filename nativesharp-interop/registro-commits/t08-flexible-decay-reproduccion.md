# T08-010/014: conversión pendiente de miembro flexible a puntero

En la revisión `24c602b6c`, el control existente de arreglos flexibles terminó
con código 0 y ejecutó sus casos positivos O0–O3. El script conserva dos sondas
informativas que se rechazan; su salida cero no significa que estén soportadas.

`ProbeDecay.ns` intenta `int* first = value->data` sobre `FlexInts*`, cuyo
campo C es `int data[]`. NSC rechaza la inicialización: no convierte
`builtin:int[*]` a `builtin:int*`. La salida está en
`A:/nsc-interop-build/tanda08-flexible-c/ProbeDecay.log`.

El acceso por índice ya toma la dirección del campo en HIR; eso no demuestra
que una conversión implícita preserve calificación, alineación o vida útil.
Se requiere trazar y probar semántica y lowering antes de modificar la regla.
