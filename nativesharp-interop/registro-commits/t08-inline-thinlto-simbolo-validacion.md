# T08-003: referencia de objeto inline C a NativeSharp

Con `nsc` de desarrollo build `be32a31d134f5d52`, el control ejecutado mediante
un llamador CMD con `call` terminó con código 0 en O0–O3. Una función C inline
llamó a `HostCallback`, definido como `export extern C` en NativeSharp; el
ejecutable comprobó el resultado 42. En O1–O3 el control verificó que el índice
ThinLTO estuviera presente; O0 usa el código normal indicado por el compilador.

Se incorpora el control al validador de la tanda. Esta prueba cubre un símbolo
exportado en un ejecutable directo: no acredita todas las referencias de
objetos nativos, bibliotecas precompiladas ni el transporte por sidecar. La
validación completa de la tanda sigue pendiente.
