# Suma y resta decimal con saturación e inexactitud

Las operaciones `saturating` de suma y resta decimal utilizaban el resultado antiguo de
64 bits cuando el coeficiente alineado no cabía en 96 bits y aún quedaba escala distinta de
cero. `Max − 0.1` es interior al rango, pero carece de representación decimal exacta;
no puede sustituirse por el límite ni por un valor truncado. En la reproducción de esta
rama el proceso terminó con 31 en O0 y O2; la salida contractual es `DecimalOverflow` (145).

Se compara la magnitud matemática exacta a su escala alineada con `Max96 × 10^escala`.
La implementación satura únicamente cuando es mayor que ese límite, conserva el signo y
pone escala cero. Si el resultado está dentro del rango y no cabe tras las reducciones
exactas, emite el trap; si cabe, conserva el resultado exacto. Con escalas válidas 0–28,
el cálculo en 320 bits cubre las magnitudes alineadas sin desbordar. El cálculo añadido
puede iterar hasta 28 veces por operación y su costo no se ha medido.

La decisión de no redondear suma/resta sigue `TAREAS-CHAT-NUMERICOS.md`; la regla half-up
histórica corresponde a división. La corrección no altera políticas `checked` ni default,
los ejemplos originales, otros subsistemas ni los archivos reservados al integrador central.
El alcance y restricciones generales se conservan en `TAREAS-CHAT-NUMERICOS.md`.

La línea base y resultados están en `A:/nsc-limitaciones-numericos-evidencia`:

- `baseline-saturating`: `PositiveInner1Add` devolvió 31 en O0 y O2, en lugar de 145.
- `fix-saturating`: 25 proyectos independientes, en O0 y O2, terminaron con el código
  esperado: interiores 145, exteriores y controles exactos 0. Los logs de compilación y
  ejecución se conservan por caso. Los controles comprueban también los bits de valores
  representables de signos y escalas diversos.
- `checked-saturating-control`: `VerifyDecimalInexactChecked.cmake` pasó las 18
  combinaciones O0/O2 de las políticas default y checked.
- `control-decimal_signed_add_sub`, `control-decimal_wide_add_sub`,
  `control-decimal_exact_scale_reduction` y `control-decimal_saturating_scalezero`:
  el ejecutor `VerifyDecimalGenericArray.cmake` terminó 0 en cada caso, O0/O2.

Se construyó incrementalmente el target `nsc` en `A:/nsc-limitaciones-numericos-build`.
El ejecutable usado en estas pruebas tiene SHA-256
`540c8f782da35f2692b0fff18f569863e0b96c13cafd09d5b2186022a297729f`; runtime e imports
proceden de ese build en A, con NSLink externo. Estas pruebas son dirigidas y no certifican
todo decimal ni una batería completa. La integración del runner en `CMakeLists.txt` requiere
un cambio del integrador central; no se modificó aquí el archivo compartido.

Análisis dirigido `clang-tidy` con `clang-analyzer-*` sobre `OopLlvmOperators.cpp`: salida 0,
con nueve avisos de padding en cabeceras existentes no modificadas; registro
`A:/nsc-limitaciones-numericos-evidencia/static-saturating-filtered.log`. La primera base
de compilación extraída de Ninja incluía una orden de regeneración además de la compilación y
falló; se conservó ese log y se filtró a la única invocación de `clang-cl` antes de repetir.
No hay línea base comparable del analizador anterior al cambio.
