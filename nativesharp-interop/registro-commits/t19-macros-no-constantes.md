# T19-041: macros de objeto C cuya expansión no es una constante

Fixture nuevo: `pruebas-tanda-19-macros-no-constantes/validar.py`, con y sin `--dll`. Contrato en
`build-claude-t08/tools/t19c/d18/t19-041-contrato.md`.

## Defecto

Una macro de objeto C cuya expansión no es una constante (`errno`, `(*api_errno())`; la dirección de una global
`dllimport`; una llamada; un record devuelto o apuntado) no se importaba: «does not expand to a scalar constant that
NativeSharp can import». Clang la acepta en cada uso.

## Decisiones

- **Accesora C.** Para cada macro de objeto C del encabezado que no es constante escalar, ni valor constante, ni tipo
  (T14, T19-003), la sesión prueba en un análisis aparte:
  - `static inline __typeof__(M)* __nsc_macro_ref_M(void) { return &(M); }`, si la expansión es un lvalue;
  - si no, `static inline __typeof__((0, (M)))* __nsc_macro_get_M(void)`, que guarda el valor en una variable
    `static` y devuelve su dirección.

  Las aceptadas se agregan a la fuente; un designador de función o `void` sigue sin importarse, con su motivo.
- **Global con accesora.** La macro se importa como global C de enlace interno con su nombre y el tipo apuntado
  (`cMacroAccessor`, `macroAccessor`, `cppMacroAccessor`); de solo lectura si es un valor (`cppMacroValue`). MIR marca
  el campo estático y LLVM obtiene su dirección llamando a la accesora en cada acceso, como un campo estático importado.
  Así cada lectura, escritura, `++`, campo y `&` evalúa la expansión una vez, como en C. La accesora se exporta con el
  símbolo de la global, solo si el programa usa la macro.
- **Precompilado.** El catálogo C lleva una marca final opcional para una global C (3 y el nombre C de la accesora),
  tras el tipo, como la marca de lenguaje de T19-036; su USR es el de la accesora.
- **Sin TLS.** El valor de una macro rvalue se guarda en una variable `static`, no `_Thread_local`: un programa
  NativeSharp no enlaza el TLS del CRT (`_tls_index`). Límite: dos hilos que leen la misma macro rvalue a la vez
  comparten esa copia.
- Solo C; C++ no cambia. No se modificó Clang ni NSLink.

## Rechazos genuinos, también en Clang

- `NEXT_ID = 1;`: «left side of assignment is not assignable» (Clang: «expression is not assignable»).
- `&NEXT_ID`: «cannot take the address of C macro 'NEXT_ID': its expansion is a value, not an lvalue» (Clang:
  «cannot take the address of an rvalue»).

## Validación

- `pruebas-tanda-19-macros-no-constantes`, estática y DLL: NativeSharp y C devuelven 608586, O0-O3, directo y desde
  la plantilla precompilada con y sin encabezado; 17 llamadas a accesoras en el IR de NSC; ambos rechazos.
- `pruebas-tanda-19-macros` (T19-003), estática y DLL, y `pruebas-tanda-14-macros`, estática: pasan.
- Evidencia: `A:/temp/nsc-t19-macros-objeto-by19x640`; logs `build-claude-t08/logs/regresion-d67.log` y
  `regresion-dll-d67.log`.
