# Emisión de ejecutable NSC con TLS C y LLD

Se prepara un proyecto que declara la biblioteca C y `libcmt.lib` como
dependencias y selecciona LLD explícitamente mediante `NSC_LINKER`. La matriz
O0–O3 debe comprobar el flujo de emisión, enlace y ejecución de NSC con el
control TLS entre hilos. Con NSC build `0934f50429ac4352` la matriz pasó
O0–O3; `llvm-readobj --coff-tls-directory` confirmó un directorio TLS no vacío
en O0. Esto no demuestra selección automática, `nsc build` ni TLS directo
desde DLL. La prueba original con NSLink permanece en la batería.
