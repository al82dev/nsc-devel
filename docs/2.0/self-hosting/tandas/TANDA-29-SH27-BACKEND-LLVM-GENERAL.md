# NativeSharp 2.0 self-hosting — tanda 29: backend LLVM general de proyecto

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH27` |
| Rama | `development-claude` |
| Fecha | 2026-09-20 |
| Depende de | tanda 28 / SH26 |
| Commit de entrada | `7348f4d03` |
| Commit de cierre | este commit (`selfhost(tanda-29): generalize the project LLVM backend`) |

## 2. Resultado

`--emit-project-object` dejó de compilar únicamente `FirstSourcePath`. Construye el `SourceGraph`,
selecciona las unidades del propietario raíz en orden canónico y entrega el programa completo al
frontend y a un único módulo LLVM. Una fixture de tres unidades enlaza y ejecuta con LLD y NSLink
en O0, O1, O2 y O3; las ocho ejecuciones devuelven 42.

El backend eliminó los arreglos fijos de cuatro argumentos. `ScalarFunctionType` y el lowering de
llamadas reservan con `stackalloc` exactamente el número de parámetros o argumentos, y el gate
ejecuta `Sum6(a,b,c,d,e,f)` cruzando unidades.

## 3. ABI y mangling

El nombre histórico `__nativesharp_source_<simple-name>` colisionaba entre namespaces y overloads.
`LlvmFunctionName(SyntaxTree, SemanticModel, declarationId)` usa ahora:

```text
identity = qualified-semantic-name + "|" + nominal-function-type
coff-name = "__nativesharp_" + StableHash(identity) + "_" + source-name
```

`StableHash` ya es el hash contractual del compilador; no depende del proceso, locale, ruta ni
orden del filesystem. Declaración, definición, llamadas y búsqueda de `Main` usan la misma función.
Las llamadas prefieren el símbolo resuelto por semántica y sólo conservan búsqueda textual como
compatibilidad para facts todavía incompletos.

## 4. Archivos y métodos

| Archivo | Cambio |
|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | mangling semántico; firmas y llamadas sin límite fijo de cuatro |
| `selfhost/nsc/Driver/Program.ns` | `EmitProjectBackend`: programa raíz completo a objeto |
| `selfhost/tests/backend-general/*` | proyecto de tres unidades y llamada de seis argumentos |
| `selfhost/contracts/backend-general.contract.json` | contrato SH27 |
| `selfhost/bootstrap/verify_backend_general.py` | símbolos, estabilidad y resultados de ejecución |
| `cmake/VerifyNativeSharpBackendGeneral.cmake` | matriz O0–O3 × LLD/NSLink |
| `CMakeLists.txt` | registro de `nsc.selfhost.backend-general` |

## 5. Decisiones y límites localizados

- El programa raíz se materializa hoy como un módulo LLVM único. No hay wrappers por fixture ni
  nombres especiales; la partición por unidad y enlace incremental pertenece a SH28/SH39.
- Los `ProjectReference` continúan fuera del módulo y entran por metadata, contrato fijado en SH26.
- El backend aún baja directamente desde sintaxis/semántica; hacer que consuma sólo IR validado es
  exactamente SH28 y no se presenta como cerrado aquí.
- La fixture cubre la ABI escalar Win64 vigente. Aggregates, `ref/out/in`, varargs y ABI extendida
  tienen sus gates SH29 y SH33.

## 6. Evidencia dirigida

`nsc.selfhost.backend-general`: **PASSED**, 12.58 s.

- cuatro compiladores candidatos, O0–O3;
- cuatro objetos de proyecto completo;
- mangling idéntico entre perfiles para `Main`, `Sum6` y `Twice`;
- ausencia de `__nativesharp_source_`;
- ocho ejecutables, LLD y NSLink por perfil, todos con código 42.

- [x] Backend de proyecto real, no primer archivo.
- [x] Firmas y llamadas con más de cuatro argumentos.
- [x] Mangling semántico estable.
- [x] O0–O3 con LLD y NSLink.
- [ ] Baseline completo acumulado al terminar las tandas restantes.
