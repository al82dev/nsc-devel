# T19-040: macros de función C con `#` y `##` llamadas desde NativeSharp

Fixture nuevo: `pruebas-tanda-19-macros-cadena-pegado/validar.py`, con y sin `--dll`. Contrato en
`build-claude-t08/tools/t19c/d18/t19-040-contrato.md`.

## Defecto

T19-003 expande una macro de función C con un envoltorio `static inline` cuyos parámetros reciben los valores
NativeSharp. Una macro que convierte un argumento en cadena (`#x`, también a través de otra macro) o lo pega a otro
token (`a##b`) escribía o pegaba el parámetro del envoltorio (`__nsc_a0`), no el argumento de la llamada, así que se
rechazaba: «the macro turns an argument into a string (#)» y «the macro pastes an argument into another token (##)».
Clang las acepta (`STR(a + b)`, `CALL(one)`, `CAT(1, 2)`).

## Decisiones

- **Parámetros de tokens.** Un parámetro que la macro usa solo con `#` o `##`, o que no usa, es de tokens
  (`macroTokenParameters`, al leer las macros de la sesión). Como en C, su argumento no se evalúa: NativeSharp no lo
  analiza, no lo baja a HIR y no lo pasa; Clang recibe su texto en el envoltorio. Así `REG(CTRL)` llega a `REG_CTRL`,
  que solo existe tras pegar, y `FIRST(3, nada)` acepta un nombre que no existe en NativeSharp.
- **Texto del argumento.** Los tokens NativeSharp del argumento, con un espacio donde la fuente los separaba, como C
  escribe un argumento para `#` (`ProjectCompilation::argumentSpelling`). Un `#` escapa `\` y `"`.
- **Valor también convertido en cadena.** Si Clang acepta el envoltorio y una cadena de su cuerpo es exactamente un
  parámetro (`"__nsc_a0"`, o `"(*__nsc_a0)"` para `ref` en C), la sesión preprocesa el envoltorio, cambia esa cadena
  por el texto de la llamada y lo analiza otra vez (`expandStringifiedWrapper`). Cubre `#x` directo (`CHECK(v > 2)`) y a
  través de otra macro (`XCHECK(v * 2)`, `XSTR(v + 1)`).
- **Un envoltorio por llamada.** Cuando la expansión depende del texto, la clave del envoltorio incluye el texto y la
  llamada guarda su envoltorio (`CppTemplateInstantiation::cMacroWrapper` → `CppMacroCallWrapper`); el análisis
  elige solo ese candidato y quita los argumentos de tokens. Una llamada cuyos argumentos son todos tokens se baja
  como llamada sin argumentos.
- **`, ##__VA_ARGS__`** (elisión de coma de GNU) es un uso de valor: `LOGV(0)` y `LOGV(2, 3, 4)`.
- **Sin uso.** Una macro sin `#` ni `##` (`SQ`) sigue el camino de T19-003 sin cambios; el fixture de T19-003 y el
  de la tanda 14 pasan.
- No se modificó Clang ni NSLink.

## Rechazos

- Genuino, también en Clang: un pegado que da una función que no existe (`CALL(three)` → `fn_three`), «Clang cannot
  expand the C/C++ function-like macro 'CALL'».
- Se conservan con su motivo (límites, Clang sí las acepta):
  - `#__VA_ARGS__` o `##` sobre los argumentos variables: NativeSharp pasa los argumentos variables solo como
    valores;
  - `#x` que, a través de otra macro, convierte en cadena más que el argumento (`#define S(x) STR(x + 1)`): conserva el
    rechazo «turns an argument into a string» de T19-003.

## Validación

- `pruebas-tanda-19-macros-cadena-pegado`, estática y DLL: NativeSharp y C devuelven 771532, O0-O3, directo y
  desde la plantilla precompilada con y sin encabezado; 22 llamadas a envoltorios en el IR de NSC.
- `pruebas-tanda-19-macros` (T19-003), estática y DLL, y `pruebas-tanda-14-macros`, estática: pasan.
- Evidencia: `A:/temp/nsc-t19-macros-cadena-rm9ugw0d`; logs `build-claude-t08/logs/regresion-d66.log` y
  `regresion-dll-d66.log`.
