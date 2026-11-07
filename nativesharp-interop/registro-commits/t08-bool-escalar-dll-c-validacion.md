# T08: validación de global C bool exportado por DLL

Se ejecutó `nativesharp-interop\pruebas-tanda-08-record-c\dll-bool\validar.cmd`
desde `A:\nsc-devel`. El control C independiente pasó y el consumidor
NativeSharp pasó en O0, O1, O2 y O3. Se verificó en el LLVM IR que los accesos
volátiles del global exportado usan i8 con acquire/release. El fixture se agregó
a la validación agrupada de la tanda 8, que todavía no se ha repetido.

Esta evidencia cubre ese caso concreto; no cierra T08-004, T08-015 ni la tanda 8.
