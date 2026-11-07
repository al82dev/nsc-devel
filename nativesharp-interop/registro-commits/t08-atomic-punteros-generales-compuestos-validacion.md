# T08-004: aritmética de punteros atómicos

El consumidor NativeSharp asignó, restó y sumó un elemento a punteros
atómicos internos a struct C y bool; C observó los valores esperados. Las
variantes estática y con referencia DLL pasaron O0–O3. El IR contiene
actualizaciones secuencialmente consistentes, alineadas a ocho bytes, con
desplazamientos -4, -1, 1 y 4. Evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-2461-6399`.

La construcción fue incremental. Pasaron regresiones de punteros atómicos
de anchos adicionales, globales y TLS, y de apuntados calificados. El control
C utiliza asignación para alcanzar el mismo estado, ya que Clang rechaza la
forma compuesta sobre un puntero atómico C. No acredita todas las operaciones,
tipos o combinaciones de concurrencia; la tanda sigue abierta.
