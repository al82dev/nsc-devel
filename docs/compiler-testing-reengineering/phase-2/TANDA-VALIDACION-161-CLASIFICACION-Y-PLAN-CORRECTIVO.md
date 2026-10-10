# Validacion 161: clasificacion y plan correctivo de la corrida integral

## SCOPE LOCK

- Autoridad: `_nscbuilder3/history/20261010-142113-a514d35c`.
- Revision ejecutada: `31bed3f92f248d756399718a22235560d7b1a1bc`.
- Seleccion: `Total`, `Debug`, NativeSharp `O0`, perfil `Todos`.
- Universo ejecutado: `5,695` variantes.
- Universo correctivo: `274 FAIL`, `4 INFRASTRUCTURE_ERROR` y `161 BLOCKED`.
- Los `41 KNOWN_FAILURE` permanecen visibles y catalogados, pero no se cuentan como
  fallos nuevos de esta corrida.
- No se cambia el compilador para satisfacer una prueba equivocada.
- No se cambia una expectativa correcta para ocultar un defecto del compilador.
- Cada causa se corrige en un commit pequeno con su Markdown y prueba puntual.
- Al cerrar cada tanda se reejecuta el conjunto correctivo de `439` variantes.
- La bateria completa de `5,695` variantes se reserva para el cierre de todas las
  tandas y sera ejecutada por el usuario.

## Contabilidad causal cerrada de los 274 FAIL

| Clasificacion | Casos | Accion |
|---|---:|---|
| Defecto de prueba u oraculo | 215 | Corregir fixture, precondicion o expectativa contra el contrato vigente. |
| Defecto de infraestructura | 4 | Entregar un prefijo instalado real al gate BuildSystem. |
| Defecto real confirmado del compilador | 5 | Corregir producto con una hipotesis causal por commit. |
| No diagnosticable con la evidencia actual | 50 | Conservar valores observados, repetir puntualmente y reclasificar antes de tocar producto. |
| **Total** | **274** | No existen filas omitidas. |

## Defectos confirmados ajenos al compilador

### Frontend, Parser, Sema y Compilation: 130

- `65` casos Frontend reutilizan seis fuentes invalidas como si fueran validas:
  `for` sin bloque (`40`), tres formas de `move x` sin parentesis (`15`),
  `throw;` fuera de `catch` (`5`) y `some IValue` donde `IValue` fue declarado
  `interface` en lugar de `trait` (`5`).
- `35` casos llegan a su oraculo despues de que `expectProjectStage` vacia la
  fuente de la variante de ausencia; un source vacio no puede publicar los
  simbolos y modelos que el mismo caso exige.
- `8` casos macro usan `macro/add!`; el motor vigente reconoce `#macro/#expand`.
- `4` oraculos Parser contradicen el contrato vigente: `TP-PAR-026`, `169`,
  `189` y `200`.
- `7` fixtures/oraculos Sema son incorrectos: `TS-SEMA-167`, `193`, `237`,
  `238`, `240`, `266` y `311`.
- `11` oraculos Compilation son incorrectos: `TF-COMPILATION-100`, `209..216`,
  `334` y `381`.

### HIR, MIR, SSA, Passes y Dominance: 35

- `11` casos MIR usan un `Main` incompatible o bucles sin bloque y fallan antes
  de producir MIR.
- `TF-MIR-267` construye un operando sin definicion y exige un diagnostico
  posterior distinto al primer error causal.
- `TF-HIR-166`, `188`, `207` y `208` construyen, respectivamente, un string
  truncado por NUL, un prototipo valido esperado como invalido y dos stores con
  destino no puntero.
- `TF-DOM-030` no corrompe la variante que despues espera rechazar.
- `TF-PAS-273..280` colocan PHI despues de instrucciones Constant, fuera de la
  forma SSA que consume el pase.
- `TF-PAS-393..397` y `399` buscan `constant`; el opcode contractual es `const`.
- `TF-SSA-134` y `198` cuentan stores de simbolos address-taken como si fueran
  candidatos promovibles.
- `TF-SSA-268` y `294` aplican mutaciones que no cambian el modelo y despues
  esperan rechazo.

### Driver, Interop, LanguageService, BuildSystem y SelfHost.Basic: 50

- Driver (`18`): version invalida que impide llegar a la capability buscada,
  comparacion textual fragil de JSON, texto de progreso equivocado, estado de
  cancelacion consultado despues de `wait()` y sentinel inexistente en la guia.
- Interop (`16`): el ABI report publica `identityKey`, no `fullyQualifiedName`,
  y la clasificacion de un diff vacio es `identical`, no `compatible`.
- LanguageService (`14`): fixtures ya formateados o con texto imposible,
  conteo de simbolos que incluye parametros y cancelacion confundida con fallo.
- BuildSystem (`1`): `T12B-00453` quita `LLVM_DIR`, pero no aisla la busqueda y
  CMake vuelve a descubrir correctamente LLVM.
- SelfHost.Basic (`1`): `TSH-00019` usa vectores Latin-1 aunque `string` y
  `StableHash.Text` recorren bytes UTF-8.

## Infraestructura: 4 FAIL y 161 consumidores

`T12B-00025`, `00026`, `00029` y `00030` reciben `build-clang` como si fuera
un prefijo instalado. Por ello no existe
`share/nativesharp/toolchain/NativeSharpInstallation.files`. El gate debe
recibir o materializar un layout instalado real; copiar el arbol de build no
es una instalacion.

Los productores `TSH-00188`, `00399`, `00849` y `00981` fallaron en la corrida
integral y bloquearon `161` consumidores. Los commits posteriores
`803d0d035`, `283ce8887`, `92ecf47ee` y `7131a7ab7` aportan evidencia dirigida
de productor en verde. Los consumidores siguen pendientes de reejecucion y no
se declaran aprobados anticipadamente.

## Defectos reales confirmados del compilador: 5

1. `TP-PAR-020`: `Parser::scanTypeAt` consume `borrowed`, pero no su
   `[lifetime]`, aunque `parseType` si lo hace.
2. `TP-PAR-024`: el lookahead generico no descompone `GreaterGreaterEqual` al
   cerrar niveles genericos; `TP-PAR-098` ya corrobora la limitacion.
3. `TP-PAR-160`: un promoted field duplicado produce `ExpectedDeclaration` en
   lugar de `DuplicateSymbol`.
4. `TP-PAR-170`: un variant case duplicado produce `ExpectedDeclaration` en
   lugar de `DuplicateSymbol`.
5. `TF-HIR-120`: parser y sema aceptan `ConditionalExpression`, pero
   `HirBuilder::lowerExpression` carece de esa rama y lo rechaza como nodo no
   soportado.

Los casos MIR `TF-MIR-152`, `183`, `195`, `198`, `200` y `270` dependen del
quinto defecto. Se tratan como `BLOQUEADO_POR TF-HIR-120`, no como seis defectos
MIR independientes.

## Casos que requieren evidencia antes de decidir: 50

- Frontend (`6`): `TF-FRONT-161`, `162`, `163`, `164`, `167`, `168`.
- Sema (`26`): `TS-SEMA-100`, `111`, `116`, `118`, `123`, `125`, `127`,
  `128`, `132`, `135`, `140`, `145`, `152`, `157`, `158`, `161`, `168`,
  `173`, `204`, `208`, `214`, `216`, `219`, `220`, `222`, `285`.
- Dominance (`11`): `TF-DOM-024`, `032`, `040`, `072`, `080`, `104`, `128`,
  `216`, `224`, `232`, `256`; el timeout no identifica si el costo esta en el
  builder, verifier u oraculo.
- SSA (`1`): `TF-SSA-273`; falta conservar el veredicto y error observados.
- MIR bloqueados por HIR (`6`): `TF-MIR-152`, `183`, `195`, `198`, `200`,
  `270`.

## Tandas de implementacion

| Tanda | Alcance | Cierre puntual | Cierre de tanda |
|---|---|---|---|
| 1 | Infraestructura BuildSystem y revalidacion de productores/consumidores self-host | 4 BuildSystem, 4 productores y 161 consumidores | Reejecutar las 439 variantes correctivas. |
| 2 | Helper de variante vacia, macros y seis fuentes Frontend | Cada causa y contrato afectado | Reejecutar las 439 variantes correctivas. |
| 3 | Oraculos confirmados de Compilation, Parser, Sema, HIR, MIR, SSA, Passes y Dominance | Cada contrato corregido | Reejecutar las 439 variantes correctivas. |
| 4 | Oraculos confirmados de Driver, Interop, LanguageService, BuildSystem negativo y SelfHost.Basic | Cada contrato corregido | Reejecutar las 439 variantes correctivas. |
| 5 | Cinco defectos reales de Parser/HIR, respetando el productor HIR antes de sus seis consumidores MIR | Un defecto por commit y prueba puntual | Reejecutar las 439 variantes correctivas. |
| 6 | Ampliacion de evidencia para los 44 casos aun no diagnosticables que no dependen del HIR ya corregido | Evidencia observada por caso y reclasificacion causal | Reejecutar las 439 variantes correctivas. |
| 7 | Defectos de producto adicionales que demuestre la tanda 6 | Un defecto causal por commit | Reejecutar las 439 variantes correctivas. |

Si la tanda 6 no descubre defectos adicionales, la tanda 7 se cierra como no
aplicable. Una vez cerradas todas las tandas se solicita al usuario una corrida
integral nueva `Debug/O0/Todos`; no se proclama cierre global antes de ella.

## Estado inicial

- Inventario causal: `274/274` clasificado.
- Filas omitidas: `0`.
- Producto modificado durante esta auditoria: no.
- Pruebas ejecutadas durante esta auditoria: no.
- Siguiente accion: Tanda 1.
