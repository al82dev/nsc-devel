# T08-002: typedef C equivalentes en encabezados distintos

## Causa y corrección

Clang incluye el nombre del encabezado en los USR de typedef de ámbito de
archivo. NSC trataba el cambio de nombre como conflicto incluso cuando los
tipos canónicos coincidían. La resolución reconoce ese formato de USR y compara
además las identidades conocidas de records y enums subyacentes. Las demás
formas de USR y los tipos canónicos incompatibles conservan su rechazo.

## Validación

- Construcción incremental de `nsc`, `nsc_cpp_c_identity_tests` y
  `nsc_cpp_session_tests` con Clang 23.1.0; la primera compilación pasó con
  advertencias preexistentes en la prueba de sesión. Se recompilaron los
  objetivos tras ajustar la sonda de identidad.
- `pruebas-tanda-08-identidad-c/validar.cmd` con salida aislada en
  `A:/nsc-interop-build/tanda08-identidad-h1-20261107b`: código 0 y
  `VALIDACION_CALL_OK`. La matriz ejecutó sus tres variantes; aceptó los
  typedef equivalentes y rechazó las definiciones incompatibles de tipo o
  record. La sonda verifica dos sesiones con `a.h` y `b.h`.
- El validador contiene también rechazos esperados de otros casos abiertos;
  su resultado no certifica el cierre de T08-002 ni de la tanda 8.

La aplicación de la regla depende del formato de USR de Clang 23.1.0. La
sonda histórica de nombres con `@` comprueba el texto del USR, pero aún no
prueba ese caso mediante el compilador completo. La batería completa de la
tanda queda pendiente.
