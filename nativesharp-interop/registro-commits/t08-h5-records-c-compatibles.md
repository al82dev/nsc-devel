# T08-002 H5: miembros de records C compatibles en dos encabezados

## Causa y corrección

Aunque el grupo de records había verificado compatibilidad estructural y el
selector comparaba los miembros, también exigía que las definiciones vinieran
del mismo archivo. Por ello omitía los campos de dos definiciones C compatibles
en encabezados distintos. El selector acepta esas observaciones cuando el grupo
ha establecido compatibilidad para transporte por puntero, las declaraciones
C tienen el mismo USR conocido y los miembros coinciden. Las definiciones C++
mantienen la regla anterior de origen; los grupos incompatibles siguen rechazados.

## Evidencia

- Compilación incremental de NSC e identidad: código 0.
- `pruebas-tanda-08-identidad-c/validar.cmd`: código 0. La matriz aceptó el
  proyecto con records iguales y rechazó el de definiciones distintas; mantiene
  otros rechazos esperados que no cierran el ítem.
- La sonda `T11SameTagFields` ahora ejecuta una lectura con valor comprobado.
  Sus variantes de un encabezado, dos nombres distintos y encabezados homónimos
  pasaron O0–O3; salida en `A:/nsc-interop-build/tanda08-identidad-h5-20261107/typedefs`.
- `nsc_cpp_session_tests` con Clang 23.1.0: código 0.

Falta una prueba específica del mismo caso desde una biblioteca precompilada.
No se ejecutó la batería completa ni se cierra T08-002 o la tanda 8.
