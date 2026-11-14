# T09-003: fábricas sobrecargadas en plantillas precompiladas

Una plantilla que llamaba una fábrica C++ sobrecargada y utilizaba un método del objeto retornado no podía reconstruir ese método cuando las alternativas devolvían propietarios distintos. La selección conserva ahora todos los propietarios candidatos cuya identidad, USR, símbolo y referencia se verifican; no publica un subconjunto cuando un candidato no puede proyectarse. La resolución de la sobrecarga y la comprobación de argumentos permanecen en el consumidor. No cambia el contrato NSMeta.

## Validación

- La misma plantilla se instanció con `int` y `long`, seleccionando respectivamente `makeOver(int) -> Probe*` y `makeOver(long long) -> Other*`. Ambos métodos se ejecutaron con resultados distintos, con y sin encabezado, en O0–O3, con biblioteca estática y DLL.
- Clang 23.1.0 rechazó como ambigua la llamada `makeTie(1, 1)` a las firmas `(int, long long)` y `(long long, int)`. NativeSharp también rechazó el empate al instanciar la plantilla, con y sin encabezado, en las mismas configuraciones. El argumento `null` incompatible también se rechazó.
- Los controles de reasignación, escape y procedencia conservan sus rechazos o ejecuciones esperados. La prueba de identidad con Clang real pasó; las cinco pruebas seleccionadas de CTest pasaron. La regresión de métodos estáticos con consumidores precompilados pasó.
- Evidencia íntegra: estática `A:/temp/nsc-t09-instance-object-size-5qn2ml6b`, DLL `A:/temp/nsc-t09-instance-object-size-pk1t68zp`, métodos estáticos `A:/temp/nsc-t09-size-static-method-sqwe6v0p`.

## Límites

La selección exige que todas las alternativas tengan aridad exacta y no sean variádicas, con retorno de record y procedencia verificable. No acredita argumentos predeterminados, candidatos no proyectables, otros flujos de receptor ni todas las formas de sobrecarga o ABI. Las asignaciones se admiten únicamente si sus posibles propietarios están incluidos en los del inicializador y pasan las demás comprobaciones conservadoras. T09-003 y la tanda 9 siguen abiertos.
