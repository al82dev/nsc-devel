# Validación de TLS transitivo

Se incorpora la prueba de dependencias TLS transitivas al validador de la
tanda 8. La compilación incremental de NSC terminó correctamente y la prueba
individual pasó con emisión directa O0–O3 y con `nsc build` automático.
La tanda sigue abierta: los controles TLS de DLL aún presentan fallos y la
batería integral no se repitió para este cambio aislado.
