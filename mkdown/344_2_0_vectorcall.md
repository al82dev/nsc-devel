# Ítem 344 — `vectorcall` / Vectorcall

## Resultado / Result

NativeSharp incorpora `extern vectorcall delegate` como function pointer Microsoft x64. El thunk
de entrada se declara con `x86_vectorcallcc`, mientras su llamada al cuerpo NativeSharp conserva la
convención interna; la invocación indirecta aplica la misma convención al call site. / NativeSharp
adds `extern vectorcall delegate` as a Microsoft x64 function pointer. Its entry thunk is declared
with `x86_vectorcallcc`, while the call into the NativeSharp body keeps the internal convention;
the indirect invocation applies the same convention at the call site.

## Contrato publicado / Published contract

NSMETA sube a 1.20 y reserva el bit 1 de `delegateFlags` para `vectorcall`; un downlevel 1.19 se
rechaza para evitar pérdida silenciosa. Los headers C/C++ emiten `NS_VECTORCALL` y lo expanden a
`__vectorcall` en toolchains Microsoft. / NSMETA advances to 1.20 and reserves delegate flag bit 1
for `vectorcall`; a 1.19 downlevel is rejected to prevent silent loss. C/C++ headers emit
`NS_VECTORCALL` and expand it to `__vectorcall` on Microsoft toolchains.

## Pruebas / Tests

La ejecución usa seis argumentos `float`, de modo que los dos últimos distinguen vectorcall de la
convención C ordinaria, y exige 42 en O0/O2. También verifica IR, header, round trip NSMETA y rechazo
downlevel. / Execution uses six `float` arguments, making the final two distinguish vectorcall from
ordinary C calling convention, and expects 42 at O0/O2. IR, header, NSMETA round trip, and downlevel
rejection are also verified.
