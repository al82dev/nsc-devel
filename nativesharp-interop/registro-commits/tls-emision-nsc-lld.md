# Emisión de ejecutable NSC con TLS C y LLD

Se prepara un proyecto que declara la biblioteca C y `libcmt.lib` como
dependencias y selecciona LLD explícitamente mediante `NSC_LINKER`. La matriz
O0–O3 debe comprobar el flujo de emisión, enlace y ejecución de NSC con el
control TLS entre hilos. La selección, la construcción y la prueba todavía no
están validadas; no demuestra selección automática, `nsc build` ni TLS directo
desde DLL. La prueba original con NSLink permanece en la batería.
