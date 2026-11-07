# T08: validacion dirigida de punteros atomicos C

La construccion incremental de NSC termino correctamente. Los controles global y TLS para punteros
a signed char, unsigned char, unsigned short y unsigned long long pasaron O0, O1, O2 y O3;
ambos comprobaron el IR atomico. Paso tambien `pruebas-tanda-08-tls-c/atomic/validar.cmd`.
La construccion mostro dos advertencias de inicializadores en CppSemanticInventory.cpp.
Los controles nuevos se incorporan al validador de la tanda; no se ejecuto la bateria completa.
No se demuestra aislamiento por hilo para los cuatro tipos ni soporte de apuntados calificados.
La tanda 8 y T08-004/015 siguen abiertos.
