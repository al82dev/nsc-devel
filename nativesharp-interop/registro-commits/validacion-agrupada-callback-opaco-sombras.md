# Validación agrupada de callbacks, tipos opacos y sombras

## Construcción

Con el código hasta `69799eece`, se ejecutó construcción incremental en
`A:/nsc-interop-build/tanda1-ON` de los targets `nsc` y
`nsc_cpp_session_tests`, con `--parallel 4`. Terminó con código 0 y el
compilador informó el build `3f82235fd35e7b9a`. La comprobación de runtime
estático del compilador pasó. Hubo advertencias de inicializadores parciales
en archivos existentes.

`ctest --test-dir A:/nsc-interop-build/tanda1-ON -R nsc.cpp-interop.session
--output-on-failure` terminó 1/1. Incluye los controles preparados de
identidad del callback con puntero a record.

## Proyectos mixtos

- `pruebas-tanda-08-callback/validar.cmd`: código 0. Compiló, enlazó y
  ejecutó los casos preparados de callbacks, incluidos el acceso directo y el
  callback con puntero a record, en O0–O3; también ejecutó los controles
  negativos y la comparación de IR incluidos en el script.
- `pruebas-tanda-08-incompleto/validar-opaco.cmd`: código 0. Ejecutó las
  muestras opacas y el control de aritmética sobre `Opaque**` en O0–O3,
  además de los casos de metadata y los controles negativos preparados.
  Los logs de `+=`, incremento e índice contienen el diagnóstico semántico
  `pointer arithmetic requires a complete C type`.
- `pruebas-tanda-08-meta-c/validar-sombras.cmd`: código 0. Ejecutó en O0 los
  casos de sombra preparados, incluida la sombra local de un global y un
  enumerador C.

Son validaciones agrupadas dirigidas a estos cambios, no la batería integral
de la tanda 08. No cierran T08-017, T08-018 ni T08-019: siguen faltando otras
firmas, colisiones, operaciones y combinaciones.
