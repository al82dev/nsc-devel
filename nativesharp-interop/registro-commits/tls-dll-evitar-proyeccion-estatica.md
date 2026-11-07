# Evitar proyección estática de TLS procedente de DLL

El fixture directo demostró que `extern __declspec(thread) int` de una
referencia con `Runtime="api.dll"` se proyectaba como TLS del ejecutable.
Esa ruta usa el índice TLS del módulo incorrecto. En `provideGlobalsWithLookup`
se examinan las referencias propietarias de declaraciones y alias; si alguna
tiene runtime y el candidato es TLS, se informa que el contrato aún no está
implementado. Un índice de referencia inválido también se rechaza.

No cambia NSLink ni la proyección TLS sin runtime. La comprobación de DLL,
la regresión estática y la validación agrupada quedan pendientes de ejecución;
este diagnóstico no constituye interoperabilidad de DLL.
