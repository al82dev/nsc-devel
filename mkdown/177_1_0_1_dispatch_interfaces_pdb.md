# PDB-P2: dispatch de interfaces, virtuales y fronteras ABI

## Resultado

El productor CodeView/PDB conserva el camino real de ejecución de NativeSharp. Una llamada virtual
o de interfaz muestra el método lógico escrito por el usuario; la búsqueda de descriptor, mapa de
interfaces y slot permanece inline en el llamador y no crea un frame artificial. Las únicas
rutinas publicadas como `S_THUNK32` son fronteras que realmente adaptan o protegen un ABI.

## Inventario del lowering

| Pieza emitida | Forma física | Clasificación PDB |
|---|---|---|
| Implementación virtual | función del método apuntada por el vtable | `S_GPROC32`, nombre lógico |
| Implementación de interfaz | función concreta apuntada directamente por la tabla de métodos | `S_GPROC32`, nombre lógico |
| Búsqueda de interfaz | descriptor → mapa → id estable → slot, inline en el llamador | sin procedimiento ni frame adicional |
| Handler estático | dirección del método con receiver nulo en la celda delegate | `S_GPROC32`, nombre lógico |
| Handler de instancia | dirección del método y receiver en la celda delegate | `S_GPROC32`, nombre lógico |
| Guard de callback C | función `__c_boundary` con la firma externa exacta | `S_THUNK32`, artificial |
| Wrapper `export extern C` | entrada C que reenvía al método NativeSharp y contiene la excepción | `S_THUNK32`, artificial |
| Cctor, destrucción, filtro tipado y destrucción de closure | comportamiento generado propio | `S_GPROC32`, artificial, no thunk |

No existe hoy un thunk de interfaz separado: `ProjectOopMirDispatchTable::InterfaceTable::calleeNames`
contiene los nombres de las implementaciones concretas y el backend coloca esas funciones en la
tabla. Añadir wrappers sólo para que aparezcan en el debugger mentiría sobre el stack y alteraría la
identidad de puntero de función.

## ABI de structs

El fixture cubre simultáneamente:

- `Pair`, de 8 bytes, transportado directamente por Win64;
- `WideValue`, de 12 bytes, transportado indirectamente y retornado mediante `sret` físico;
- las firmas PDB permanecen a nivel fuente (`Pair`/`WideValue`), sin exponer el puntero oculto;
- los parámetros y locales conservan el tipo nominal y sus campos/offsets.

## Evidencia dirigida

`tests/integration/debug_pdb/Main.ns` ejecuta dispatch virtual, dispatch de interfaz, handlers
estático/de instancia, delegate C protegido, un struct directo y uno indirecto. El resultado O0/O2
continúa siendo 42.

`cmake/VerifyNativeSharpDebugArtifacts.cmake` exige:

- métodos lógicos como `S_GPROC32` y ausencia de clasificación `S_THUNK32` falsa;
- guards/wrappers C como `S_THUNK32`;
- tipos nominales de 8 y 12 bytes con firmas fuente;
- líneas O0 de ambas llamadas de dispatch y ambos caminos de aggregate ABI;
- `.pdata` y registros unwind con `UnwindInfoAddress` (lld-link puede plegar `.xdata` en `.rdata`);
- ejecución y PDB válidos en O0 y O2.

Las suites semánticas dirigidas que acompañan el gate son Stage 36 (interfaces), Stage 40.2
(aggregate ABI), Stage 42 (delegates/eventos) y Stage 56 C1 (fronteras C).
