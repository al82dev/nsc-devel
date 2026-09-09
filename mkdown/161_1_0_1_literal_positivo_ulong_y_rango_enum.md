# 161 — NativeSharp 1.0.1: literal positivo `ulong` y rango enum / Positive `ulong` literal and enum range

## 1. Hallazgo del baseline

La primera corrida completa posterior a las expresiones constantes de enum encontró una regresión
que las pruebas enfocadas no mostraron:

```nativesharp
public enum Tope : ulong
{
    Cero = 0,
    Max = 18446744073709551615
}
```

El compilador decía que `Max` no cabía en `builtin:ulong`. El mismo error impedía llegar al
diagnóstico correcto de un miembro implícito colocado después de `ulong.MaxValue`.

La evidencia roja quedó en la corrida `20260908-204545-ec8fea99`: seis fallos derivados del mismo
caso, repartidos entre CTest y fixtures con LLD y NSLink. G0.2 certificó que el árbol no se movió,
pero aplicó la regla 13 y no lo marcó `LANDED`.

## 2. Causa exacta

`builtins::integerLiteralTypeKey` devolvía `builtin:long` para todo literal positivo sin sufijo que
superara `int.MaxValue`. Al entrar `18446744073709551615` en el evaluador tipado, la normalización
de `long` interpretaba sus bits como `-1`. `ProjectMemberModel::collectEnumMembers` recibía entonces
un valor negativo y lo rechazaba correctamente para un enum unsigned, pero el tipo del literal ya
era incorrecto.

La implementación previa a `5a9ed611` no observaba el tipo intermedio y por eso el caso había
compilado. Adoptar el evaluador compartido expuso la omisión de la frontera `long`/`ulong`.

## 3. Archivos y funciones modificados

### Compilador

- `include/nativesharp/Frontend/IntegerLiteralTyping.h`
  - `integerLiteralTypeKey` distingue ahora las cotas positivas de `long` y `ulong`;
  - conserva la cota especial de `-9223372036854775808` como `long` cuando el menos unario es
    directo;
  - agrega aserciones de compilación para el bit 63 positivo y `ulong.MaxValue` sin sufijo.

### Pruebas

- `tests/unit/Cc1IntegerEvaluatorTests.cpp`
  - verifica que `uint64_t::max()` sin sufijo conserve tipo `builtin:ulong` y todos sus bits.
- `tests/cc1/c38_enum_ancho/Main.ns`
  - ya contenía la prueba ejecutable canónica y detectó la regresión sin necesitar un workaround.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - ya exigía que un miembro posterior a `ulong.MaxValue` produjera “there is no next one to count
    to”; volvió a alcanzar ese diagnóstico después de la corrección.

No se modificaron las dos últimas pruebas para esconder el fallo: se corrigió la regla compartida
que ambas ejercen.

## 4. Decisión de implementación

La regla permanece centralizada en `IntegerLiteralTyping.h`. No se añadió un caso especial dentro
de enums porque eso habría hecho que el mismo literal tuviera tipos diferentes en un enum, un
campo `const`, una etiqueta de `switch` y una expresión ordinaria.

Para un literal sin sufijo se elige `int`, después `long` y finalmente `ulong` según la magnitud.
El menos unario directo conserva las cotas asimétricas de los mínimos con signo. Los sufijos
explícitos `u`, `l` y `ul` no cambian.

## 5. Verificación

Pruebas enfocadas después del arreglo:

- LLD: 3/3 aprobadas (`cc1.integer-evaluator`, `c38_enum_ancho`, `cc1.correctness`);
- NSLink: 3/3 aprobadas;
- una primera ejecución concurrente de los dos agregados chocó en el directorio temporal compartido
  `%TEMP%\nsc-cc1-rest`; LLD se repitió en solitario y pasó. No se contó ese choque como fallo del
  compilador.

Baseline definitivo:

```text
run_id:     20260908-210330-3a9390d3
estado:     LANDED, certified=true
duración:   682.304 s
CTest:      306/306 LLD, 306/306 NSLink
fixtures:   148/148 LLD, 148/148 NSLink
triage:     18/18 LLD, 18/18 NSLink
fallos:     0
```

## 6. Commit de referencia / Reference commit

### Título en español

`Corrige el tipo de literales positivos que requieren ulong`

### English title

`Fix positive literals that require ulong`

### Descripción en español

Tipa como `ulong` los literales positivos sin sufijo que exceden `long.MaxValue`, conserva la regla
especial de los mínimos negativos y agrega cobertura directa para `ulong.MaxValue`. Restaura tanto
la ejecución de enums de 64 bits como el diagnóstico de que no existe sucesor después de su máximo.

### English description

Type unsuffixed positive literals above `long.MaxValue` as `ulong`, preserve the special rule for
negative minima, and add direct `ulong.MaxValue` coverage. Restore both 64-bit enum execution and
the no-successor diagnostic after the maximum value.
