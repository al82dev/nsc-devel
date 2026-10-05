# NS3 baseline: identificador compatible en la fixture LLVM self-hosted

## Problema

`selfhost/tests/llvm/EmitMinimalObject/Main.ns` llamaba `i32` a una variable que contiene un
`LLVMTypeRef`. En NativeSharp 3.0, `i32` forma parte del vocabulario reservado de tipos y ya no se
puede analizar como identificador ordinario. Esto rompía el bridge LLVM y, por dependencia, las
comparaciones del frontend y varios cierres self-hosted.

## Cambio

La variable se renombró a `int32Type` en sus cuatro usos. El tipo LLVM creado, su ABI y el objeto
emitido no cambian.

## Archivos

- `selfhost/tests/llvm/EmitMinimalObject/Main.ns`

## Mejora

La fixture usa la gramática pública vigente de 3.0 y vuelve a poder ser procesada tanto por el host
como por las generaciones self-hosted sin introducir una excepción léxica para código de prueba.
