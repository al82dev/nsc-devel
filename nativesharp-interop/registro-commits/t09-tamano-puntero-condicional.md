# T09: tamaño oculto de una selección de punteros

Una selección `condicion ? p : q` se rechazaba en los modos de tamaño de
subobjeto 1 y 3. NSC admite ahora esa forma cuando la condición es un booleano
local, parámetro o constante y cada rama es un puntero local o parámetro sin
lectura volátil. Usa el puntero resultante ya evaluado para consultar su
tamaño; las ramas y condiciones con otras expresiones siguen pendientes.

## Validación

- Construcción incremental de `nsc` y verificación del runtime del host.
- `validar-puntero-desplazado.py --solo-condicion`: los ocho modos se comparan
  contra Clang con ambas ramas, punteros a reservas de distintos tamaños,
  punteros a campos distintos y una rama nula. El contador comprueba una sola
  evaluación de la condición. Estática y DLL, O0–O3, consumidores directos y
  precompilados con y sin encabezado, en convenciones predeterminada y SysV.
  C++: `A:/temp/nsc-t09-pointer-offset-pz6951kf` y
  `A:/temp/nsc-t09-pointer-offset-ras8i77u`; C:
  `A:/temp/nsc-t09-pointer-offset-egzd54i2` y
  `A:/temp/nsc-t09-pointer-offset-7cj5delt`.
- `--solo-condicion-parametro`: ambas ramas con condición y punteros
  recibidos como parámetros, en las mismas configuraciones. C++:
  `A:/temp/nsc-t09-pointer-offset-3fyphs4z` y
  `A:/temp/nsc-t09-pointer-offset-glt7hyxi`; C:
  `A:/temp/nsc-t09-pointer-offset-esl72gn4` y
  `A:/temp/nsc-t09-pointer-offset-quogry5c`.

## Límites

Las pruebas cubren las selecciones indicadas en Windows x64. Las ramas que
calculan direcciones o tienen efectos, otras condiciones y convenciones no
quedan acreditadas. T09-003 permanece abierto.
