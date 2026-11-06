# Callback con puntero a record C y biblioteca precompilada

El fixture prepara una biblioteca NativeSharp que publica callbacks C de
firma `int(CallbackPayload*)`, un holder C y un callback leído del global.
Un consumidor con el mismo header C usa los tres caminos. El script prepara
la biblioteca C y la matriz O0–O3 en `A:/nsc-interop-build`.

Esta preparación no demuestra que la firma ni sus dependencias sobrevivan a
la metadata; la validación está pendiente.
El script acepta `--diagnostico` para limitar una reproducción a O0; sin ese
argumento conserva la matriz O0–O3 para la validación agrupada.

Con el build `3f82235fd35e7b9a`, el diagnóstico O0 produjo la biblioteca y su
NSMETA, compiló y enlazó el consumidor y ejecutó los tres accesos con código 0.
O1–O3 y otras firmas permanecen pendientes; esta muestra no cierra T08-017.

Se preparó un segundo consumidor que no incluye el header: infiere el puntero
al record, el callback y el holder desde la API publicada por el productor.
El consumidor con header permanece como control.

Con el build `3f82235fd35e7b9a`, el diagnóstico O0 compiló, enlazó y ejecutó
los consumidores con y sin referencia explícita al header, con resultado 0.
En esa primera ejecución el archivo seguía disponible en el directorio de trabajo; esta prueba
no demuestra que el consumo funcione después de retirarlo. O1–O3 siguen
pendientes de la matriz agrupada.

Se preparó una comprobación más fuerte: el script oculta la copia de `Api.h`
en el directorio de pruebas después de generar el productor, intenta compilar
el consumidor y restaura la copia incluso si el compilador devuelve error.
El header fuente permanece intacto.

El diagnóstico O0 terminó con código 0 con la copia usada por el productor
temporalmente ausente. Al terminar, `fc /b` confirmó que la copia restaurada
coincide con el header del repositorio y no quedó el archivo oculto. La fuente
del repositorio seguía presente; no se afirma aislamiento de todo el sistema
de archivos. O1–O3 siguen pendientes de la matriz agrupada.
