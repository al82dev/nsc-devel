# Contraste de arranque CRT en DLL con TLS

Con el mismo `ScalarApi.c`, el guion `validar-escalar-crt.cmd` enlaza la DLL
con CRT estático y `_DllMainCRTStartup`, en lugar de `/noentry`. El control C
de inicialización cero pasó; NativeSharp leyó y escribió los seis escalares
TLS en O0–O3. No se cambió el código de NSC para obtener el resultado.

Las variantes anteriores sin CRT inicializado se conservan y siguen fallando
en el control C. La [documentación de Microsoft sobre `/NOENTRY`](https://learn.microsoft.com/en-us/troubleshoot/cpp/linker-warning-managed-extension-c-dll-project)
describe la necesidad de inicialización cuando una DLL usa bibliotecas que
dependen de estáticos. Esa referencia ayuda a interpretar el fixture, pero no
prueba por sí sola el mecanismo concreto del fallo observado. Tampoco se ha
verificado que toda DLL con CRT estático funcione ni se ha cerrado la tanda.
