# División decimal: ancho, signo y límites

La línea base `A:/nsc-limitaciones-numericos-evidencia/baseline-division-expanded/observations.txt`
registra 30 discrepancias O0/O2: pérdida de signo, truncamiento a 64 bits, excepción de
hardware al dividir por un coeficiente no nulo con los 64 bits bajos en cero, falta de
reducción exacta y ausencia de manejo de overflow. El control half-up `0.5/3` sí pasó.
Las reproducciones y la política adoptada están identificadas en el registro del carril.

## Decisiones de contrato

Los documentos históricos fijan half-up y trap de escala mayor de 28; dejan sin establecer
la política de overflow de división y no fijan el trap del divisor decimal cero. Conforme
a la autorización delegada se eligió:

- Un coeficiente divisor cero produce `DivisionByZero` (129 en este runtime) antes de
  evaluar la escala, incluso si está codificado con signo o escala distintos de cero.
- Conservar el target histórico `max(sa,sb) + (sa > sb ? 1 : 0)` y half-up sobre la
  magnitud; el signo del resultado no cero es XOR y el cero se normaliza a signo positivo.
- Si el coeficiente redondeado supera 96 bits, retirar únicamente ceros decimales exactos
  mientras haya escala. Si no cabe después, default/checked produce `DecimalOverflow` (145).
- En `saturating`, limitar a ±Max solo si el valor matemático exacto está fuera de rango;
  un valor interior que no sea representable exactamente tras redondear produce 145.

La precedencia del cero y las políticas de división son decisiones nuevas, no hechos
atribuidos a los contratos históricos. No se cambia la política de otras operaciones.

## Implementación y límites

Se extraen los 96 bits de ambos coeficientes y se enmascaran ambas escalas. El numerador
`Ca × 10^(target + sb − sa)` cabe en 320 bits para escalas válidas: el exponente máximo
es 56. Una división restauradora de 320 pasos genera cociente y residuo sin `UDiv` ancho;
`2 × residuo >= divisor` aplica half-up. Para saturación se contrasta el cociente sin
redondear y su residuo contra `Max × 10^target`. La reducción por diez usa diez fragmentos
de 32 bits y divisiones por diez de 64 bits, con resto menor que diez. El costo de las 320
iteraciones y de hasta 28 reducciones no se ha medido.

Los helpers locales preservan la creación y ordenación de bloques LLVM del compilador y
no requieren cambios en la cabecera reservada. Una línea de multiplicación previa se dividió
por formato, sin cambio semántico. El alcance general se conserva en
`TAREAS-CHAT-NUMERICOS.md`; el registro de tests compartido queda para el integrador central.

## Verificación dirigida

El target `nsc` se construyó incrementalmente en `A:/nsc-limitaciones-numericos-build`.
Ejecutable SHA-256 `027ff35778a24ad9e83ed31a6b8beb803f321f6b1bd8dde5c0f4dddd3f3a7053`;
runtime e imports proceden del mismo build. NSLink es externo y no se instaló nada.

- `final-division/observations.txt`: 28 proyectos de multiplicación y división, cada uno
  compilado y ejecutado O0 y O2; 56 resultados esperados y cero discrepancias. Incluye signos,
  96 bits, divisor cero, medio exacto, debajo/encima del medio, overflow, escala y reducción.
- `final-division-exponent56/observations.txt`: exponente 56 y 28 reducciones exactas,
  aprobados O0 y O2 por separado.
- `decimal-division-differential-20261112T123238Z-75d17a3853/results.json`: 60 divisiones
  checked representables generadas con semilla 349091 y verificadas O0/O2 contra un oráculo
  entero. Conserva casos, comandos, salidas, hashes y una copia `generator.py`. Excluye divisor cero y coeficientes
  no representables; no demuestra exhaustividad, traps ni saturación.
- `final-saturating`: 50 resultados esperados de suma/resta O0/O2. Los runners `final-*`
  de i128/u128 y `final-decimal-checked` terminaron 0. El fixture de matemática determinista
  pasó con Core fuente local; no certifica consumo binario del SDK.
- `originales-ejecucion-20261112T123133Z-8455c910d5/results.json`: ejemplos 48 y 49 con
  resultados esperados O0/O2, en las variantes Core fuente identificadas en el manifiesto.

La matriz principal se ejecutó desde el repositorio mediante:

```text
A:\nsc-limitaciones-numericos-temp\run.cmd cmake -DNSC_EXECUTABLE=A:/nsc-limitaciones-numericos-build/bin/nsc.exe -DNSLINK_EXECUTABLE=nslink.exe -DFIXTURE_DIR=A:/nsc-limitaciones-numericos/tests/integration/decimal_mul_div_observe -DOUTPUT_DIR=A:/nsc-limitaciones-numericos-evidencia/final-division -DRUNTIME_LIBRARY=A:/nsc-limitaciones-numericos-build/generated/NativeSharp.Runtime.Win32.lib -DSOURCE_DIR=A:/nsc-limitaciones-numericos -P cmake/VerifyDecimalMulDivObserve.cmake
```

El ejecutor registra para cada proyecto `--emit-project-oop-exe`, la optimización, salida
individual, `--no-artifact-cache` y `--no-error-log`; las compilaciones y ejecuciones se
registran por separado. El caso de exponente máximo usó el mismo comando con
`OUTPUT_DIR=.../final-division-exponent56` y `CASE_PREFIX=DivExponent`. El diferencial se
ejecutó mediante `python -X utf8 -B A:\nsc-limitaciones-numericos-temp\decimal-division-differential.py`
con el entorno aislado en A del wrapper; su JSON conserva los comandos exactos.

Los ejecutables dirigidos compilaron y enlazaron, incluidos operandos anchos; esto no prueba
la ausencia de helpers externos para todas las entradas o configuraciones. El análisis
`clang-tidy` con `clang-analyzer-*` terminó 0 y mostró nueve avisos de padding en cabeceras
no modificadas (`static-division-final.log`). No existe línea base comparable. No se ejecutó
una batería completa; los resultados se limitan a las configuraciones descritas.
