# Evidencia parcial: TLS de C, tanda 8

Con `nsc` de desarrollo `f6b7ed644963e398`, la construcción incremental de
`nsc` y `nsc_cpp_session_tests` terminó correctamente. Se comprobó:

- El control C y el acceso de NS a escalares TLS estáticos pasaron O0–O3,
  incluyendo aislamiento entre hilos; el IR preserva cargas y escrituras
  `volatile`, y una escritura `const` se rechaza.
- La biblioteca C con TLS estático pasó O0–O3 usando LLD explícito y CRT
  estático; el global entero y el rechazo de puntero TLS también pasaron.
- El caso TLS interno de DLL accedido mediante funciones pasó O0–O3.
- La regresión de callbacks C pasó O0–O3 con NSLink; el ejecutable de pruebas
  de sesión Clang terminó correctamente.

El fixture de acceso directo a TLS exportado desde DLL continúa fallando en
el enlace de su consumidor C, antes del caso NativeSharp. La tanda 8 no está
validada ni cerrada. No se modificó NSLink ni se desplegó en C:.
