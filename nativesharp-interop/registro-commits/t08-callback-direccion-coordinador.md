# Dirección de una función C importada como callback fuera del punto de entrada

## Problema

Convertir una función C importada ordinaria (sin TLS) en un puntero a función C fallaba en LLVM cuando la
conversión estaba en una función distinta de `Main`:

```text
nsc: error: LLVM code unit #0 failed: lowering failed: C1.3 callback boundary target is unavailable: take_plain
```

Reproducción mínima (sonda en `A:\nsc-interop-build\claude\sondas\addr`, O0, encabezado directo):

```ns
int CasePlain() { PlainFn f = take_plain; return f(get_plain()); }
int Main() { return CasePlain() == 4 ? 0 : 1; }
```

La misma conversión dentro de `Main` sí compilaba y se ejecutaba. Ninguna de las dos versiones usa `volatile`.

## Causa

- **HECHO COMPROBADO.** Con más de una función, el backend divide el módulo en unidades. La unidad #0
  (`NativeSharp.OOP.globals`, coordinadora) ejecuta `OopLlvmImplementation::defineCallbackGuardThunks`, que
  recorre los cuerpos de **todas** las funciones alcanzables y define un thunk de protección (C1.3) por cada
  destino de `DelegateCreate` / `LambdaCreate`. El thunk llama a su destino, por lo que el destino debe estar
  declarado en la coordinadora.
- **HECHO COMPROBADO.** Las declaraciones de la coordinadora salen de
  `ProjectOopFunctionReachabilityAnalysis::coordinatorFunctionDeclarations`
  (`src/LLVM/ProjectOopFunctionAnalysis.cpp`): definiciones alcanzables no importadas, referencias de
  inicializadores de campos estáticos y tablas de despacho. Una función C importada solo referida como destino
  de callback desde otra unidad no entraba en ese conjunto, y `callbackGuardThunk` no la encontraba en
  `functions_`.
- **HECHO COMPROBADO.** Con un solo cuerpo no hay partición (`code-units: 1`), por eso el caso en `Main`
  funcionaba. El caso TLS variádico ya probado (`__nsc_cpp_tls_import_*`) no usa thunk y tampoco fallaba.
- La función libre `projectOopCoordinatorFunctionDeclarations` de `ProjectOopFunctionReachability.cpp` no tiene
  llamadores en `src`; no participa en la reproducción y no se modificó.

## Corrección

`src/LLVM/ProjectOopFunctionAnalysis.cpp`:

- `collectCallbackTargetNames` (nuevo, espacio anónimo): recorre con pila explícita los nodos bajo una raíz y
  reúne el `calleeName` de cada `DelegateCreate` / `LambdaCreate`. Sin recursión; O(nodos).
- `coordinatorFunctionDeclarations`: después de seleccionar las definiciones alcanzables, agrega los destinos
  de callback de esos mismos cuerpos (cuerpo y regiones de limpieza), el mismo conjunto que recorre
  `defineCallbackGuardThunks`.

Efectos: la coordinadora solo gana declaraciones de funciones que ya son destinos de thunks que ella misma
define. Las demás unidades, el MIR, los thunks y el código emitido para otros programas no cambian. La
identidad de caché de la coordinadora cambia únicamente cuando aparecen esas declaraciones nuevas.

Alternativa descartada: no envolver con thunk los destinos C importados. Cambia el contrato C1.3 de todos los
callbacks y amplía el alcance; no es necesaria para corregir la causa demostrada.

## Pruebas

Nueva: `pruebas-tanda-08-identidad-c/funcion-direccion-c/validar.py` (O0–O3 por defecto). Cuatro casos, cada
uno en su propia función distinta de `Main`: invocar la dirección de una función importada, pasarla a una
función C que la invoca, dos importaciones distintas y un local de tipo callback pasado a C. Cada caso se
compila con encabezado (directo) y mediante plantilla precompilada consumida sin encabezado, y se ejecuta. Un
control C compilado con Clang confirma los valores esperados.

| Prueba | Resultado |
|---|---|
| Sonda `caseplain.ns` antes de la corrección | falla con `callback boundary target is unavailable` |
| `funcion-direccion-c/validar.py` (O0–O3, directo y precompilado) | correcto |
| `funcion-direccion-c/validar.py` antes de la corrección | falla en directo O0 con el mismo diagnóstico |
| Regresiones de callbacks (ver `t08-callback-volatile-identidad.md`) | ver ese registro |

Construcción incremental de `nsc` en `A:\nsc-interop-build\tanda1-ON`; sin batería completa.

## No verificado

- Otros nodos que tomen direcciones (`PollAddress`, tablas) ya estaban cubiertos por la coordinadora; no se
  revisaron más allá de la lectura del código.
- Callbacks `vectorcall` de funciones importadas en unidades partidas.
