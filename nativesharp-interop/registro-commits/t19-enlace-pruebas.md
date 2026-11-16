# Tanda 19: enlace de las pruebas de NSC (T19-037)

## Defecto

En la [validación de la tanda 18](../TANDA-18-VALIDACION-AGRUPADA.md#pruebas-de-nsc-ajenas-a-interoperabilidad-t18-014),
83 pruebas de CTest quedaban `Not Run` porque su ejecutable no se enlazaba. `ninja -k 0` termina con tres
símbolos sin definir:

| Símbolo | Lo llama | Lo define | Origen de la llamada | Ejecutables |
|---|---|---|---|---|
| `PrecompiledSemanticSnapshotCodec::decode` | `ProjectCompilationSnapshots.cpp` (`nsc_frontend`) | `nsc_precompiled` | `9c37aa6d0` (2026-10-23), anterior a la interoperabilidad | 76 |
| `cppCInlineLinkSymbol`, `projectCppCxxEnumConstant` | `CppCRecordMetadataCodec.cpp` (`nsc_precompiled`) | `CppSemanticInventory.cpp` (`nsc_cpp_interop`) | `ed29a2453` y `68e931a8c` (2026-11-07, tanda 8) | 7 |

En cada caso la biblioteca que llama no declara la biblioteca que define. `nsc.exe` enlaza porque `nsc_driver`
lista las tres; un ejecutable de prueba que enlaza solo `nsc_frontend` o `nsc_precompiled` no las recibe.

## Cambio

`CMakeLists.txt`:

- `target_link_libraries(nsc_frontend PUBLIC nsc_precompiled)`. Las dos bibliotecas son estáticas y ya se
  dependían en sentido inverso; CMake repite las dos al enlazar para resolver el ciclo.
- `target_link_libraries(nsc_precompiled PUBLIC $<BUILD_LOCAL_INTERFACE:nsc_cpp_interop>)`, dentro del bloque
  que define `nsc_cpp_interop`. `nsc_cpp_interop` no se exporta. Con `PUBLIC nsc_cpp_interop` o con
  `$<BUILD_INTERFACE:…>`, la generación falla (`export called with target "nsc_precompiled" which requires
  target "nsc_cpp_interop" that is not in any export set`). `BUILD_LOCAL_INTERFACE` limita la dependencia al
  árbol de construcción.

No cambia ninguna fuente, prueba ni opción de compilación.

## Evidencia

- `ninja -C build-tanda09 -k 0`: 0 errores de compilación y 0 símbolos sin definir (antes, 3). Registro:
  `logs/t09-build-k0.log`.
- CTest completo (`ctest-completo.cmd lb1`), comparado con la tanda 18 (`comparar-ctest.py`):

  | | Tanda 18 | Ahora |
  |---|---|---|
  | Pasan | 515 | 592 |
  | Fallan | 106 | 112 |
  | Sin ejecutar | 83 | 0 |

  - De las 83 que no se ejecutaban, 77 pasan.
  - Las otras 6 se ejecutan y fallan en comprobaciones de diagnóstico:
    - `nsc.project-construction-model`;
    - `nsc.project-object-layout-model`;
    - `nsc.stage35.recovery`;
    - `nsc.stage36.recovery`;
    - `nsc.stage42.delegates-events`;
    - `nsc.stage45.exception-sema`.

    No tienen línea base y siguen en T19-037.
  - Ninguna prueba que pasaba en la tanda 18 falla ahora.
- `nsc.exe` se volvió a enlazar. La regresión de interoperabilidad (`lote-t19-todo.cmd lb1`) da lo mismo que
  la validación agrupada de la tanda 19:
  - **Estática:** pasan las tandas 19 a 10, 67 de 67. La tanda 9 da 1095 de 1099: fallan los dos
    `cpp-variadica-preserve-none` (T19-032) y se omiten los dos de siempre.
  - **DLL:** 66 de 67. `cpp-tamano-objeto/validar` falla con cinco procesos y pasa repetido con uno
    (`logs/regresion-dll-lb1r`, T19-026).
  - **`lote-ct2`:** las nueve CTests de interoperabilidad y `nsc_cpp_c_identity_tests` pasan.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
