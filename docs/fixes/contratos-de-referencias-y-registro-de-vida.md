# Referencias, préstamos, registro de vida entre módulos y reemplazo de dueños RAII

Fichas LKF-333, 334, 390, 416 y NSC-NCSS-004 (parcial).

## Defectos corregidos

- **LKF-416.** `ModuleLifetimeRegistry` congelaba la huella del contrato completo del primer objeto,
  que nombra su tipo y sus thunks, así que un segundo tipo exportado por el mismo módulo productor se
  rechazaba como "incompatible allocator/destruction contracts". El registro guarda ahora la huella
  de módulo (`moduleLifetimeFingerprint`: ABI, productor, asignador y política de descarga); un
  asignador distinto sigue rechazándose. TL-LANG-082 añade el caso de dos tipos.
- **NSC-NCSS-004 (parcial).** `a = a + b` sobre un local `owned`/`raii` de clase, con un operador de
  usuario que devuelve `owned C`, se rechazaba ("RAII locals are move-only"): la comprobación miraba
  la sintaxis (`CallExpression`) y no el resultado. Ahora un operador unario o binario con resultado
  owned transferible reemplaza al dueño como una llamada.

## Contratos verificados

- **LKF-333.** Una referencia inicializada desde una llamada que devuelve la referencia de un
  parámetro funciona sobre un local del llamador; devolverla cuando apunta a un local propio se
  rechaza, directa o a través de una referencia local ("ref return cannot expose storage owned by the
  current stack frame").
- **LKF-334.** Un local `borrowed` exige un dueño vivo o un identificador prestado directo; desde el
  resultado de una llamada se rechaza con ese mensaje. Es una restricción declarada.
- **LKF-390.** Un préstamo de un dueño `raii` del propio marco sobrevive a un `await` y el valor es
  correcto en O0 y O2.

## Pendiente (NSC-NCSS-002, NSC-NCSS-004)

- `a += b` sobre un local RAII de clase sigue rechazado: la asignación compuesta no transfiere el
  dueño devuelto por el operador.
- Dos temporales de un struct move-only (campo `owned`) como operandos `in` de un operador no
  encuentran sobrecarga viable, aunque dos locales sí; y un temporal no se puede pasar con `in` a un
  método ("operand of & must name addressable storage ... temporary value").

## Pruebas

- `nsc.compiler.ownership-control-contracts` (41 casos: 7 nuevos).
- Suite del compilador, TL-LANG-082 y TL-LANG-084 (huella de módulo).
