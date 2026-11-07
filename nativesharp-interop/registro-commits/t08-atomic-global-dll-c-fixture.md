# T08: global C atómico exportado por DLL

Se prepara una DLL C que exporta `_Atomic(int)`, un control C independiente y
un consumidor NativeSharp, con comprobación de IR atómico y ejecución O0–O3.
El control todavía no se ha ejecutado; no acredita la ruta ni modifica
producción o NSLink.
