# T08 (cierre): `nsc.project-oop-lowering` y `nsc.stage46.native-libraries`

Fecha: 2026-11-09. Instrucción del usuario: investigar los dos fallos de ctest y distinguir los problemas del
compilador de los de configuración o fixtures. Hay que corregir lo que corresponda y validar con el compilador de
desarrollo en `A:`, sin tocar `C:`.

## Resultado

| Prueba | Antes | Después | Causa |
|---|---|---|---|
| `nsc.project-oop-lowering` | 25 de 660 fallan | pasa | fixtures y expectativas de la prueba desactualizadas; ningún error del compilador |
| `nsc.stage46.native-libraries` | 10 de 454 fallan | pasa | 8: configuración (compilador medido en `C:`); 2: fixture que viola la regla de `throw` chequeado |

No se modificó código del compilador.

## `nsc.project-oop-lowering`

### Origen

La prueba no enlazaba desde antes de la tanda 8 porque le faltaba `nsc_precompiled`. Se volvió enlazable en
`2939a7210` (2026-11-05). Para fecharlo, se construyó la prueba fuera del repositorio (`claude\bisect`):

- en `e64631180` (2026-10-26), con solo la corrección de enlace aplicada, ya fallaban las mismas 25
  comprobaciones;
- en `2939a7210` también fallaban las mismas 25.

Los fallos son anteriores a los cambios de interoperabilidad de esta tanda. Durante las semanas en que la prueba
no se construía, el compilador cambió y la prueba no se actualizó.

Para ver la causa de cada fallo, una copia instrumentada de la prueba (también en `claude\bisect`, no en el
repositorio) imprimió el error y la forma real del HIR.

### Causas y correcciones

| Comprobaciones | Error observado | Causa | Corrección en la prueba |
|---|---|---|---|
| 19 de agregados, `With` y spread (incluida «10923 motifs») | «C7 aggregate initializer has no value layout for 'type:AggregateDepth::S0'» | Los auxiliares de la prueba agregan tipos a `namedTypes_` y marcan `built_`, pero no reconstruyen el índice de identidades. Ese índice existe desde `198b7719c` (perf-T5.2, 2026-10-21) y lo consultan `typeOrdinalForIdentity` y `typeOrdinalForTypeKey`. | `nameLookup.indexes_->rebuildTypes(nameLookup.namedTypes_)` en los seis auxiliares de `ProjectOopHirModelTestAccess` que marcan el lookup como construido, como hace `ProjectNameLookup::build` |
| `EnumToString preserves one receiver…` | «ID 129 enum ToString() cannot find the enum type» | la misma | la misma |
| `failure throws preserves Create…` y `…missing-Create diagnostic` | «failure:throws cannot resolve AllocationFailureException» | la misma | la misma |
| `the user-unary fixture builds a by-value operator target` | `compileProjectOopHir` falla: «concrete class 'Deep.Node' does not implement static trait requirement 'op_Addition(…)' from IAdd<Node>» | El fixture declara `struct Node : IAdd<Node>` sin implementar el `operator +` que exige el trait. El diagnóstico es correcto. | se añade `public static Node operator +(in Node left, in Node right)`; la prueba sigue eligiendo el único operador unario por valor |
| `32768 allocator constructions preserve…` | forma distinta: `Allocate` tiene 3 hijos | Congelamiento 3.0 C-08 (`6758083f5`, 2026-10-25): `IAllocator.Allocate(long bytes, int alignment)` recibe también la alineación | se espera el tercer argumento, una constante `builtin:int` |
| `iterative future discovery preserves the canonical physical result-field layouts` | 5 campos en lugar de 4 | Congelamiento 3.0 C-14: el future lleva un puntero a función `Drop` como quinto campo | se esperan 5 campos y el quinto `builtin:void*` |

Las demás expectativas no cambian.

## `nsc.stage46.native-libraries`

### Configuración: 8 comprobaciones

Las comprobaciones de la cadena DLL y del diamante lanzan un proceso de compilador nuevo
(`NSC_TEST_NSC_EXECUTABLE`). Ese valor es `NSC_TEST_COMPILER`, y por omisión `CMakeLists.txt` lo apunta al
compilador instalado `C:\native-sharp\bin\nsc.exe` (build `4998f7ed435b91d5`). Ese compilador no lee el NSMeta del
contrato vigente («metadata payload has trailing bytes»).

- **Corrección de configuración.** Solo afecta al árbol de construcción de `A:`; no cambia el repositorio:

  ```
  cmake -S A:/nsc-devel -B A:/nsc-interop-build/tanda1-ON -DNSC_TEST_COMPILER=A:/nsc-interop-build/tanda1-ON/bin/nsc.exe
  ```

  Es el override explícito que documenta `CMakeLists.txt` para medir otro artefacto. Script:
  `claude\scripts\test-compiler-a.cmd`.
- **Efecto.** Desde ahora ctest mide en `tanda1-ON` el compilador de desarrollo, incluidas las pruebas de
  script que usan `NSC_TEST_COMPILER`. No se modificó nada en `C:`.

### Fixture: 2 comprobaciones (DLL-P4)

El consumidor generado por la prueba hace `throw move(problem)` en `Exceptional()` sin declarar `throws`. La regla
del congelamiento 3.0 C-13 (`8d46bf5cc` y `73b03a394`, 2026-10-25) lo rechaza con razón: «neither caught here nor
declared by the enclosing callable». El fixture es del 2026-10-07. La corrección es declarar
`private static void Exceptional() throws Problem`.

## Validación

- `nsc.project-oop-lowering`: pasa (`claude\scripts\lowering.cmd`).
- `nsc.stage46.native-libraries`: pasa con el compilador de `A:` (`claude\scripts\st46.cmd`).
