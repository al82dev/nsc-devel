# TLS con `nsc build` y LLD explícito

`nsc build` reconoce LLD cuando `NSC_LINKER` identifica `lld-link`; registra
`LLD` como enlazador efectivo y rechaza discrepancias con `--linker`. NSLink
permanece predeterminado. La selección LLD sigue requiriendo configurar el
ejecutable mediante `NSC_LINKER`; no es selección automática.

Se prepara la prueba O0–O3 con el proyecto TLS y CRT declarado, incluyendo
el resultado JSON y la ejecución. La construcción y la matriz están pendientes;
TLS directo de DLL y la validación completa de tanda siguen abiertos.
