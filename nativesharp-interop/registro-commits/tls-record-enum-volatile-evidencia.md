# Evidencia de records y enum TLS volatile

Con NSC `c73dc89cc3ed0f95`, la construcción incremental y la suite de
sesiones pasaron. Los controles C y NativeSharp para struct, arreglo de struct
y enum TLS `volatile`, y struct `const volatile`, pasaron O0–O3 en biblioteca
estática y DLL con aislamiento de otro hilo. El IR estático conserva los
accesos `volatile` de cada global y se rechazó escribir el campo `const` en
ambos enlaces.

También pasaron las regresiones dirigidas de arreglos y globales TLS; un
arreglo TLS incompleto sigue rechazado. Se ajustó el verificador para comparar
símbolos completos y se repitió con resultado correcto. Persisten otros
contratos, DLL mínima y el gate integral. No se modificó NSLink ni C:.
