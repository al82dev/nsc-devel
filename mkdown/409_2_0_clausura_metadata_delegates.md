# Entrega 409 — Clausura NSMETA completa para delegates recursivos

## Problema

Una DLL NativeSharp podía exportar físicamente un método público y omitirlo de la superficie
materializable de NSMETA cuando su firma alcanzaba un delegate que formaba un ciclo nominal válido
con un struct.

El caso real es:

```text
ImDrawCallback -> ImDrawCmd* -> campo ImDrawCallback
```

El productor ya conservaba el método, el delegate y el layout. El defecto estaba en la segunda
validación usada para reconstruir contratos precompilados: volver a encontrar un delegate que ya
estaba en el conjunto `visiting` se interpretaba como tipo no materializable. Los ciclos de structs
equivalentes ya se aceptaban.

La consecuencia observable era una discrepancia:

```text
DLL contiene el símbolo
NSMETA no lo considera parte del contrato materializable
--verify-library-artifacts informa un export inesperado
```

## Corrección

### `src/Driver/Driver.cpp`

`canMaterializeContractType()` trata ahora la reentrada de un delegate como una arista nominal ya
validada. La primera visita sigue comprobando:

- versión NSMETA compatible;
- ABI de delegate conocida;
- calling convention `extern C` o ABI managed compatible;
- retorno y parámetros completos.

Únicamente la repetición dentro de la misma clausura devuelve éxito, igual que la clausura de
structs. No se agregaron excepciones por nombres de ImGui.

### `src/Interop/LibraryMetadata.cpp`

La clausura de tipos publicados recorre explícitamente el retorno y los parámetros de cada delegate.
Esas aristas no son miembros de clase ni campos de layout, por lo que antes podían detener la
clausura un nodo antes cuando alcanzaban un helper del módulo raíz.

## Prueba dirigida

`tests/unit/Stage48_2LibraryMetadataTests.cpp` agrega un productor SharedLibrary con:

- dos delegates distintos;
- delegate directo como parámetro;
- struct con campos delegate;
- puntero al struct;
- retorno del struct por valor;
- retorno por puntero;
- método que recibe múltiples delegates y devuelve el struct;
- delegate recursivo que recibe un puntero al struct que lo contiene;
- payload no raíz alcanzado exclusivamente desde la firma de un delegate.

La prueba:

1. compila el productor DLL en O0;
2. verifica `.dll`, `.lib` y `.nsmeta`;
3. inspecciona identidad, calling convention, firmas y layout;
4. exige que los ocho métodos estén publicados con identidad física;
5. compila un consumidor externo O0 mediante `LibraryReference`;
6. hace que el consumidor use las formas públicas sin `ProjectReference` ni fuentes del productor.

Resultado dirigido:

```text
nsc_stage48_2_library_metadata_tests: PASS: 292 checks
```

No se ejecutó el baseline completo por instrucción expresa del propietario del proyecto.

## Criterio cubierto

La corrección es estructural y permite eliminar workarounds basados en nombres como
`ImDrawCallback` o `ImGuiPlatformIO` una vez que el compilador corregido sea instalado y se
regenere el paquete ImGui.
