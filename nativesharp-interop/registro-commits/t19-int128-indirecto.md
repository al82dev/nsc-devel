# T19-044: `__int128` por punteros a función C, callbacks y variádicas; bit-fields `volatile` de 128 bits

Fixture nuevo: `pruebas-tanda-19-int128-indirecto/validar.py`, con y sin `--dll`.

## Defectos

Desde T19-004, una función C declarada con `__int128` usa el plan ABI de Clang: Windows x64 pasa el valor por la
dirección de una copia alineada a 16 y lo devuelve en XMM0 (`<2 x i64>`). Quedaban fuera:

- un puntero a función con un entero de 128 bits: no se mapeaba (la función que lo toma o devuelve no se importaba),
  y un callback con uno daba «C callback signature contains an unavailable type»;
- un `__int128` como argumento variádico C: «no viable»;
- una importación C variádica con un `__int128` fijo o como resultado: no tiene plan de Clang (`attachCVectorPlan`
  excluye las variádicas) y usaba el `i128` de LLVM, RCX:RDX y RAX:RDX. Este caso no estaba rechazado: `vsum(3, …)`,
  que devuelve `unsigned __int128`, daba un resultado distinto del de Clang;
- un bit-field `volatile` de un tipo de 128 bits no se importaba.

## Decisiones

Con la convención C de Win64 (`cdecl`, `ms_abi`; x64 ignora `stdcall` y `fastcall`), como Clang:

- **Llamada por puntero** (`emitExternCDelegateCall`): un entero de 128 bits fijo o variádico se pasa por la dirección
  de una copia alineada a 16, y el resultado vuelve como `<2 x i64>` y se convierte a `i128`.
- **Callback** (`createCallbackGuardThunkType`, `prepareCallbackGuardArguments`, `defineCallbackGuardThunkBody`): el
  thunk de una función NativeSharp recibe la dirección, carga el valor con alineación 16 y devuelve en XMM0.
- **Importación variádica** (`cVariadicWideIntegers`): la declaración y la llamada siguen la misma regla para los
  enteros de 128 bits fijos y el resultado; un argumento variádico se pasa por la dirección de una copia alineada.
- **Mapeo**: un puntero a función C con un entero de 128 bits se mapea con esas convenciones; con otra (SysV,
  regcall, vectorcall…) conserva el rechazo. Un alias C de `__int128` y `unsigned __int128` es `i128` y `u128`
  (`cppAliasBuiltinType`), también en una firma de callback.
- **Bit-field `volatile`** de un tipo de 128 bits: usa su unidad de almacenamiento de 16 bytes, la regla de Microsoft
  que ya seguían los de 64 bits; `big : 100` y `low : 20` comparten la unidad y se acceden con `load/store volatile
  i128`.
- No se modificó Clang ni NSLink. El contrato NSMeta no cambia.

## Rechazo genuino, también en Clang

Un puntero a función de otra firma (`IOp bad = c_twice;`): «no method in the method group exactly matches delegate
signature» (Clang: «incompatible function pointer types»).

## Validación

- `pruebas-tanda-19-int128-indirecto`, estática y DLL: NativeSharp y C devuelven 498331, O0-O3, directo y desde la
  plantilla precompilada con y sin encabezado; 9 accesos `volatile i128` en el IR; el rechazo. Cubre la llamada por
  puntero a una función C y a una NativeSharp, callbacks `apply_u` y `apply_i` con funciones NativeSharp, `vsum` y
  los bit-fields `volatile`.
- `pruebas-tanda-19-bitfields-uniones` (estática y DLL); `pruebas-tanda-08-callbacks-c` (callbacks, identidad,
  vectorcall), `pruebas-tanda-08-bitfields-c` (volatile), `pruebas-tanda-09-cpp-variadica-cdecl`,
  `pruebas-tanda-09-precompilado-variadica-anidado` y `pruebas-tanda-19-macros-cadena-pegado` (estática).
- Los scripts de la tanda 08 (`pruebas-tanda-08-callbacks-c/validar-*.py` y
  `pruebas-tanda-08-bitfields-c/validar-*volatile*.cmd`) fijan rutas de otro equipo (`A:/nsc-interop-build`,
  `A:/al82dev-clang`); se ejecutaron desde copias en `A:/temp/t08cb` y `A:/temp/t08bf` con solo esas rutas cambiadas
  al NSC y Clang actuales, sin modificar los tests. Pasan todos.
- Logs: `build-claude-t08/logs/regresion-d69.log` y `regresion-dll-d69.log`.
