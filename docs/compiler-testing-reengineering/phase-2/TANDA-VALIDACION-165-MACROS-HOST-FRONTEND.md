# Validacion 165: contrato real de macros Host.Frontend

## SCOPE LOCK

- Objetivo: corregir exclusivamente las ocho fixtures logicas del contrato
  `CF-FRONT-052` que usan la sintaxis inexistente `macro/add!`.
- Casos autorizados: `TF-FRONT-409` a `TF-FRONT-416`.
- Configuracion dirigida: `Debug`; seleccion global `O0`; las variantes host
  conservan optimizacion no aplicable (`NA`) en sus claves canonicas.
- Archivos autorizados:
  `tests/compiler-suite/host/modules/FrontendAuxiliaryModelsModule.cpp` y este
  documento.
- Archivos protegidos: compilador, `nscbuilder3.py`, `TRACKING.md`, manifiestos,
  inventarios, planes y cualquier fixture ajena a `CF-FRONT-052`.
- Grupos excluidos: `for`, `move`, `throw`, `some` y `helper`.
- Prohibido: cambiar el motor de macros, adaptar expectativas, ejecutar la
  reejecucion correctiva de `439` variantes o iniciar una corrida integral.
- Criterio dirigido: commit previo a pruebas y ejecucion posterior exclusiva de
  los ocho casos autorizados.

## Autoridad y evidencia de entrada

La corrida congelada `_nscbuilder3/history/20261010-142113-a514d35c`
registro ocho fallos de `CF-FRONT-052` en `Debug/NA`:

| Caso | Variante | Diagnostico observado |
|---|---:|---|
| `TF-FRONT-409` | 0 | faltaba un registro de expansion |
| `TF-FRONT-410` | 1 | faltaba un registro de expansion |
| `TF-FRONT-411` | 2 | faltaba un registro de expansion |
| `TF-FRONT-412` | 3 | faltaba un registro de expansion |
| `TF-FRONT-413` | 4 | el limite cero no rechazo la invocacion |
| `TF-FRONT-414` | 5 | faltaba un registro de expansion |
| `TF-FRONT-415` | 6 | faltaba un registro de expansion |
| `TF-FRONT-416` | 7 | faltaba un registro de expansion |

`FrontendContractSupport::addMatrix()` materializa ocho casos consecutivos por
contrato y entrega los indices `0..7` al mismo oraculo. Por tanto, la unica
cadena fuente de `macroOracle()` constituye la fixture compartida de las ocho
filas, sin ocho copias fisicas independientes.

## Causa raiz

`MacroExpansionEngine::expand()` descubre definiciones mediante `#macro` e
invocaciones mediante `#expand`. Sus parametros se declaran como identificadores
sin `$`; el cuerpo referencia cada parametro como `$x` o `$y`.

La fixture declaraba `macro add($x, $y)` e invocaba `add!(2, 3)`. Ninguno de
esos dos marcadores pertenece al contrato implementado. El motor terminaba con
exito sin definiciones, invocaciones ni registros. Eso explica ambas formas del
fallo: las siete variantes ordinarias recibian cero expansiones y la variante 4
no alcanzaba el gate de limite cero.

La implementacion vigente y sus pruebas unitarias independientes confirman la
forma contractual:

```text
#macro nombre(parametro) { $parametro }
#expand nombre(argumento)
```

Clasificacion: `DEFECTO_DE_PRUEBA_ORACULO`; no existe evidencia de defecto del
compilador para este grupo.

## Correccion minima

La fixture compartida conserva nombre, aridad, argumentos, cuerpo y resultado,
pero expresa las dos operaciones con el contrato real:

```text
#macro add(x, y) { $x + $y } int value = #expand add(2, 3);
```

No cambian el motor, los registros de expansion, el limite, los diagnosticos ni
las expectativas. Para variantes ordinarias debe publicarse exactamente un
registro determinista; para la variante 4, el limite cero debe rechazar la
invocacion antes de publicar una expansion.

## Validacion autorizada posterior al commit

Despues de commitear esta correccion se ejecutaran exclusivamente:

- `TF-FRONT-409`;
- `TF-FRONT-410`;
- `TF-FRONT-411`;
- `TF-FRONT-412`;
- `TF-FRONT-413`;
- `TF-FRONT-414`;
- `TF-FRONT-415`;
- `TF-FRONT-416`.

La seleccion se hara por la ruta canonica de Builder 3 con modo `Debug` y
seleccion global `O0`. No se ejecutara ningun otro caso ni la bateria completa.

## Estado previo a pruebas

- Auditoria de las ocho filas: completada.
- Correccion: implementada.
- Commit funcional previo a pruebas: pendiente de crear.
- Validacion dirigida: pendiente posterior al commit.
- Compilador y Builder 3 modificados: no.
- `TRACKING.md` modificado: no.
