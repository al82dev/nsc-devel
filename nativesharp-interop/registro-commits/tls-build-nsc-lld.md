# TLS con `nsc build` y LLD explícito

`nsc build` reconoce LLD cuando `NSC_LINKER` identifica `lld-link`; registra
`LLD` como enlazador efectivo y rechaza discrepancias con `--linker`. NSLink
permanece predeterminado. La selección LLD sigue requiriendo configurar el
ejecutable mediante `NSC_LINKER`; no es selección automática.

La prueba con NSC build `2c5d0967206b6e62` pasó O0–O3 con el proyecto TLS y
CRT declarado; el resultado JSON reportó `LLD` y cada ejecutable pasó el
aislamiento entre hilos. Un proyecto NativeSharp sin referencias C/C++ se
construyó y ejecutó con NSLink y lo reportó en JSON. TLS directo de DLL y la
validación completa de tanda siguen abiertos.
