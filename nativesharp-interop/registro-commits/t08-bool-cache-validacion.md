# T08: caché del consumidor de bool C precompilado

Se ejecutó `nativesharp-interop\pruebas-tanda-08-record-c\precompiled-bool\validar-cache.cmd`
desde `A:\nsc-devel`. En O0–O3 el productor que declara una referencia C
rechazó la caché con el diagnóstico esperado relativo a encabezados transitivos
y se compiló sin caché. El consumidor con `LibraryReference`, sin referencia C
directa y con el header C oculto, obtuvo un hit de compilación y pasó la
ejecución en los cuatro niveles. El script restaura el header después de cada
intento y se incorporó a la validación agrupada de la tanda.

Esto no demuestra caché activa para proyectos con `CppLibraryReference`, ni
invalida la protección existente. T08-004, T08-015 y la tanda siguen abiertas.
