# Baseline 2026-10-04: alineación de fixtures con Copy, move y throws

## Problema

La matriz O0 conservaba fixtures escritos antes de que NativeSharp 3.0 cerrara los contratos de
tipos `copy`, retornos move-only, spreads con transferencia y errores tipados. El compilador los
rechazaba correctamente, pero las pruebas seguían expresando las reglas anteriores.

## Cambios

- Los carriers triviales usados por ABI, flujo, parameter packs y tuplas se declaran `copy`.
- El fixture de const generics transfiere su resultado move-only con `return move(result)`.
- El fixture de destrucción declara `throws Problem` en la función que propaga ese error.
- Las expectativas de inicializadores generalizados y lambdas usan los diagnósticos actuales.

## Archivos

- `tests/unit/NativeSharp3T01TypedParameterPackTests.cpp`
- `tests/unit/NativeSharp3T01GeneralizedInitializerTests.cpp`
- `tests/unit/NativeSharp3T02NativeTupleTests.cpp`
- `tests/unit/NativeSharp3T04BidirectionalInferenceTests.cpp`
- `tests/unit/NativeSharp3T04ConstGenericTests.cpp`
- `tests/unit/NativeSharp3T06PartialMoveTests.cpp`
- `tests/integration/ns3_t39_frontend_abi/Main.ns`
- `tests/integration/ns3_t40_flow/Main.ns`

## Mejora

La batería vuelve a medir las características que anuncia cada prueba sin pedir al compilador que
copie valores move-only ni que permita propagación implícita de errores. No se relajó ninguna regla
del lenguaje.
