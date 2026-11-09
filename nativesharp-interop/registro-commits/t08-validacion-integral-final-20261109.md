# T08: validación integral tras D13b, D11/D12, D05b y D04b (2026-11-09, cierre funcional)

Compilador: `A:\nsc-interop-build\tanda1-ON`, construido con los commits hasta `1140e1bd6` (T08-D04b) y con el gate
complementario de este commit.

## Commits de este bloque

| Commit | Contenido |
|---|---|
| `18e3427bc` | T08-D13b: `volatile T*` devuelto por una función C con el modo de su referencia (NSMeta) |
| `096ea5e65` | T08-D11/D12: aviso con corrección explícita para operaciones enteras con operando C |
| `00a24c26a` | T08-D05b: enlace en ejecución del alias TLS que necesita aislar dependencias homónimas |
| `1140e1bd6` | T08-D04b: copia adaptada de una DLL C con TLS y sin directorio de importación |

## Gates

- **`validar-tanda-08.cmd` (histórico): código 0.** Registro: `claude\logs\integral-historico.log`.
- **`validar-tanda-08-complementaria.cmd`: código 0.** Ejecuta dos suites más:
  - `enteros-c\validar-divergencia-c.py` (T08-D11/D12);
  - `identidad-c\tls-sin-import-host\validar-copia-adaptada.py` (T08-D04b).

  `validar-transitivas-homonimas.py`, que ya estaba en el gate, cubre ahora T08-D05b. Registro:
  `claude\logs\integral-complementario.log`.

**Primera ejecución descartada.** La primera vez se lanzó el gate histórico a la vez que ctest. El gate construye
antes de probar, y no pudo reescribir `nsc_cpp_session_tests.exe` mientras ctest lo ejecutaba («permission
denied»). Se repitió solo, y dio código 0. El fallo era del arnés, no del compilador.

## Regresiones por bloque

| Script | Resultado |
|---|---|
| `regresion-d13b.cmd` | todo correcto salvo los dos casos conocidos de abajo |
| `regresion-d11.cmd` | todo correcto salvo `diagnosticar-var-volatile.cmd`, que termina con 1 por diseño |
| `regresion-d04b-d05b.cmd` | código 0: D04, D01, D02, D05, `validar-transitivas-homonimas.py` y `validar-copia-adaptada.py` |

## ctest (ejecutado solo, después de los gates)

**Pasan:**

- `nsc.cpp-interop.session`, `nsc.compilation-model`, `nsc.project-loader`, `nsc.project-graph`;
- `nsc.namespace-function-metadata`, `nsc.item343` a `nsc.item345`;
- `nsc.stage48-2.*`, `nsc.stage48_1`, `nsc.stage48_3`, `nsc.stage48_4`, `nsc.stage50_9`;
- `nsc.library-metadata-version-tolerance` y `nsc.library-metadata-merger`;
- `nsc.stage46-1.dll-cross-linker-matrix`, `nsc.stage47.host-c-abi` y `nsc.windows-linker`. Este último cubre el
  verificador PE que cambió en D04b.

Los ejecutables de prueba se reconstruyeron antes con `claude\scripts\build-varios.cmd`.

**Fallos que no se atribuyen a estos cambios:**

1. **`nsc.stage48_2.library-metadata`: 442 de 444.** Las dos comprobaciones leen
   `tests/fixtures/nsmeta/historical-1.46.nsmeta`, que no existe.
   - Por decisión del usuario, no se reconstruye el escritor histórico y la prueba queda intacta.
   - No se puede ejecutar completa por falta del fixture. Falla igual antes y después de estos cambios
     ([investigación](t08-d24-fixture-historico-146.md)).
   - No hay evidencia de que lo cause ningún cambio de esta tanda.
2. **`nsc.project-oop-lowering`: 25 de 660.** Son los mismos 25 que antes de D13 (agregados, `With`, spread,
   `EnumToString`, operador unario). Se comprobó con `git stash` ([detalle](t08-d13-alias-volatile-modo-origen.md)).
3. **`nsc.stage46.native-libraries`: 10 de 454.** No se había ejecutado antes en esta tanda.
   - **8 comprobaciones fallan por la herramienta medida, no por el código.** Las del consumidor de la cadena DLL y
     del diamante usan, por diseño de `CMakeLists.txt` (`NSC_TEST_COMPILER`), el compilador instalado
     `C:\native-sharp\bin\nsc.exe` (build `4998f7ed435b91d5`). Ese compilador no lee el NSMeta del contrato vigente:
     «metadata payload has trailing bytes», por el byte de modo `volatile` de D13b, entre otros.
     - No se despliega en `C:` por instrucción del usuario.
     - Con el compilador de `tanda1-ON`, los mismos proyectos que dejó la prueba se construyen. El consumidor del
       diamante importa `CompositeNative.dll` y `CompositeSibling.dll` y se ejecuta (código de salida 42, el valor que calcula).
   - **2 comprobaciones (DLL-P4) fallan por una regla del lenguaje posterior a la prueba.** El consumidor hace
     `throw move(problem)` sin `throws` y se rechaza con «neither caught here nor declared by the enclosing
     callable». Esa regla es del congelamiento 3.0 (`DeclaredErrorAnalysis.cpp`, `73b03a394` y `8d46bf5cc`, del
     2026-10-25). El fixture de la prueba es del 2026-10-07 y esta tanda no lo tocó.
4. **`nsc.item340` a `nsc.item342` y `nsc.item346` a `nsc.item349`:** sus ejecutables no están construidos en
   `tanda1-ON` y no forman parte de las pruebas de la tanda.

**Diagnóstico de diseño.** `flexible-c\diagnosticar-var-volatile.cmd` termina con 1 a propósito: compara las
referencias mixtas con los dos modos de Clang ([T08-D13](t08-d13-alias-volatile-modo-origen.md)).

## T08-D21: historial

Se conserva el historial actual y no se reescribe, por decisión del usuario (punto 6): ni squash ni push forzado,
hasta revisarlo después de terminar funcionalmente la tanda.

Respaldos verificados en `claude\respaldo\`, que se conservan:

- `development-cpp-clang-20261109.bundle`;
- `development-cpp-clang-20261109-noche.bundle`.

Esta sesión no hizo push. El reflog de `github/development-cpp-clang` registra pushes externos a esta sesión (a las 15:27 y 15:44, hora local) que publicaron la rama hasta `1140e1bd6`; el commit de esta validación y los siguientes quedan en local.

## Archivo accidental

`A:\nsc-devel\__1` ya no existe: era el archivo creado por una redirección `>` en un patrón de grep, sin
información útil (punto 7).
