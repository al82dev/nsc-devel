# Validación agrupada del bloque de identidad C

Se construyeron incrementalmente en A: `nsc`, `nsc_cpp_c_identity_tests` y
`nsc_cpp_c_anonymous_identity_tests`. Después se ejecutaron, con código 0, los
13 validadores de identidad dirigidos siguientes:

1. `homonimos/validar-global-static.cmd`.
2. `typedefs/EnumConflict/validar.cmd`.
3. `typedefs/precompiled-conflict/validar.cmd`.
4. `typedefs/precompiled-enum/validar.cmd`.
5. `anonimos-anidados/validar.cmd`.
6. `funcion-firma/validar.cmd`.
7. `sombra-funcion/validar.cmd`.
8. `plantilla-funcion/validar.cmd`.
9. `plantillas/validar-global-alias-o0.cmd`.
10. `lambda-inicializador/validar.cmd`.
11. `patron-sombra/validar.cmd`.
12. `plantillas/validar-enum-conflicto-o0.cmd`.
13. `records-precompiled/validar.cmd`.

También pasaron `validar-o0-sombras.cmd`, `validar-o0-inicializadores.cmd` y
la sonda `--typedef-matrix` sobre una copia temporal de sus datos. La sonda
imprime reglas de comparación de su propia implementación: su texto `current`
no se toma como prueba independiente del comportamiento actual de NSC. Los
records precompilados se ejecutaron de O0 a O3; los demás controles anteriores
se limitaron a O0 o análisis semántico según su propio validador.

## Ampliación de la matriz actual

Se ejecutó una copia temporal del validador de identidad en
`A:\nsc-interop-build\t08-validar-identidad-actual.cmd`, con salidas en
`A:\nsc-interop-build\t08-identidad-bloque-actual`. Terminó con código 0:

- Restauró USR de cinco variantes de productor en O0, O1, O2 y O3, mediante
  módulo precompilado y mediante el driver.
- Ejecutó 36 combinaciones de la matriz de typedefs y las muestras de records
  precompilados de O0 a O3.
- Ejecutó las matrices existentes de campos enum, plantillas y constantes,
  incluidos sus diagnósticos esperados.

La copia temporal omite la sonda general que mezcla controles históricos y la
sección de formatos históricos. Los originales y sus pruebas no se modificaron.
Por decisión del usuario, la compatibilidad entre formatos históricos de NSMETA
no se emplea como criterio de cierre. Esta es la matriz actual seleccionada,
no el validador integral original ni el gate completo de la tanda; tampoco
acredita todos los casos posibles. La tanda 8 continúa abierta.
