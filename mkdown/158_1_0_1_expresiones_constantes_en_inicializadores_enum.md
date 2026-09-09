# 158 — NativeSharp 1.0.1: expresiones constantes en inicializadores enum

## El caso real

La fuente canónica de Silk.NET declara una máscara completa así:

```csharp
ColorWriteChannels.All = Red | Green | Blue | Alpha
```

Los cuatro miembros valen 1, 2, 4 y 8; por tanto `All` vale 15. NativeSharp ya aceptaba las mismas
operaciones bitwise en expresiones normales, pero rechazaba esta forma durante el registro temprano
del enum con el diagnóstico antiguo que exigía un literal entero plano. Sustituir temporalmente la
expresión por `15` conservaba el ABI, pero ocultaba una diferencia innecesaria respecto del fuente.

## Causa

`ProjectMemberModel::collectEnumMembers` calculaba los valores antes del análisis de cuerpos y tenía
un evaluador privado mínimo: literal, menos unario y auto-incremento. El resto del frontend ya usaba
`ProjectIntegerEvaluator`, que conoce tipos, anchos, signo, casts y operaciones aritméticas y
bitwise. Había dos respuestas distintas a la misma expresión según el lugar donde aparecía.

## Corrección

El registro de enums usa ahora `ProjectIntegerEvaluator` mediante un resolvedor limitado a miembros
anteriores del mismo enum. Así `All = Red | Green | Blue | Alpha` se pliega a 15 con las mismas
reglas tipadas que un campo `const` o una etiqueta de `switch`, y la comprobación existente todavía
exige que el resultado quepa en el tipo subyacente.

La limitación del resolvedor es deliberada: no convierte llamadas, valores de runtime ni referencias
adelantadas en constantes. El diagnóstico para esas formas es:

```text
enum initializer for 'Value' must be an integral constant expression
```

## Evidencia

`Stage51EnumOopSupportTests` contiene el caso canónico y verifica que `All` se registre con valor 15.
También conserva un caso rojo con una llamada a método como inicializador. Un ejecutable enfocado,
construido por el `nsc` instalado, devolvió 42 tanto en O0 como en O2 al leer y comparar `All`.

## Seguimiento de rango unsigned

El primer baseline completo posterior encontró que el evaluador compartido tipaba
`18446744073709551615` sin sufijo como `long` antes de comprobar un enum `ulong`. La causa, la
corrección y la recertificación están registradas por separado en
`161_1_0_1_literal_positivo_ulong_y_rango_enum.md`.
