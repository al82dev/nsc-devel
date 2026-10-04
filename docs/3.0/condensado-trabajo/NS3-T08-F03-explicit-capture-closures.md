# NS3-T08-F03 — Closures con captura explicita

## Estado

TERMINADA en `development` el 2026-10-04. La evidencia es puntual O0/NSLink; no se ejecuto
baseline ni una coleccion general de pruebas.

## Contrato de lenguaje

- `[copy value]`: crea una celda independiente con un snapshot del valor.
- `[move owner]`: transfiere el valor a una celda propiedad del environment, invalida la fuente y
  ejecuta su destruccion exactamente una vez al destruir la closure.
- `[ref value]`: comparte la celda capturada para lectura.
- `[ref mut value]`: comparte la celda capturada con capacidad de mutacion.
- Una lista explicita prohibe capturas implicitas no enumeradas.
- Capturas prestadas o movimientos con prestamos vivos se rechazan por lifetime/escape.
- Una closure capturante en un callable `noheap` se rechaza porque necesita environment.

## Modelo callable

El compilador deriva y publica una capacidad equivalente a `Fn`, `FnMut` o `FnOnce`:

- solo copy/ref produce `Fn`;
- cualquier `ref mut` produce `FnMut`;
- cualquier `move` produce `FnOnce`.

El consumo de un local `FnOnce` forma parte del estado de flujo: una segunda invocacion, o una
invocacion posterior a una rama donde pudo haberse consumido, produce diagnostico.

## Representacion y ABI

MIR 1.29 conserva por captura el modo, tipo, procedencia y propiedad. El layout del environment
es visible y estable; cada slot contiene una celda. Copy/move crean una celda independiente,
ref/ref-mut retienen la celda compartida. El caller materializa el environment mediante el
allocator del modulo productor. El destructor sintetico libera valores movidos, celdas y
environment dentro de su propia funcion LLVM.

## Archivos principales

- `src/Parser/Parser.cpp`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `tests/unit/NativeSharp3T08ClosureCaptureTests.cpp`

## Verificacion puntual

Objetivo: `nsc_ns3_t08_closure_capture_tests`.

Resultado: `PASS: 7 checks` usando O0 y
`A:\nslink-devel\build-release\bin\nslink.exe`.
