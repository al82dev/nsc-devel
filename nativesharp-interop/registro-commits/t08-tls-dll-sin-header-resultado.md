# Resultado con el header temporalmente ausente

`validar-dll-precompilado.cmd` terminó con código 0 en O0–O3 después de ocultar
la copia del header durante cada compilación del consumidor y restaurarla. El
productor se compiló con el header presente; el consumidor usó la biblioteca y
su metadata. Registro en `A:/nsc-interop-build/tls-abi/dll-precompilado-sin-header.log`.
Este resultado no resuelve los controles C de DLL que siguen fallando.
