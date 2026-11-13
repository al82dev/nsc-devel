# Firma C mixta preserve_none

Se habilita la firma C `long long(long long,unsigned int,float)` con convención `preserve_none`, tanto directamente como mediante puntero. El fixture `pruebas-tanda-09-preserve-none-mixta` también conserva una firma mixta previamente probada y comprueba resultados, identidad de los punteros y convención en LLVM.

Pasaron ocho comparaciones Clang 23.1.0/NativeSharp en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-indirecta-mixta-rgqeo50m`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-indirecta-mixta-precompilado-2uo4vfi_`). La compilación incremental terminó correctamente. La muestra de O0 también verifica las ejecuciones antes y después de recuperar caché.

Este resultado se limita a las dos firmas del fixture y al destino Windows x64. Otras combinaciones, agregados, variádicas y TLS aislado requieren evidencia adicional; T09-003 y la tanda 9 siguen abiertos.
