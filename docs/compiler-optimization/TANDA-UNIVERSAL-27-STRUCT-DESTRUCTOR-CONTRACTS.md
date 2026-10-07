# Tanda universal 27: contratos actuales de destructores de `struct`

## Alcance bloqueado

- Se retiran dos expectativas negativas que precedian a T06-F02.
- Se conserva el rechazo de metodos virtuales, miembros protegidos, `copy struct` y `union` con
  destructor.
- No se modifica el compilador, ABI, NSMETA, librerias ni runtime.
- No se ejecuta baseline ni se instala nada en `C:`.

## Causa confirmada

Stage 40 y Stage 56 todavia exigian que todo destructor de `struct` fuera rechazado. El contrato
actual hace move-only a un valor destructible y T06-F02 ya verifica ejecucion exactamente una vez
en movimiento completo, parametros, retorno temprano y unwind nativo. Las dos negativas antiguas
contradecian esa cobertura positiva.

## Cambio

Se eliminan solamente `StructDestructor` y `StructDestructorRefused`. Las fronteras que siguen
siendo invalidas permanecen intactas. No se agrega una cobertura positiva duplicada porque
`NativeSharp3T06PartialMoveTests` ya prueba el comportamiento observable completo.

## Archivos

- `tests/unit/Stage40StructValueAbiTests.cpp`
  - Alinea la frontera historica con el modelo move-only actual.
- `tests/unit/Stage56C6StructMembersTests.cpp`
  - Conserva exclusivamente las negativas de virtual/protected.
- `docs/compiler-optimization/TANDA-UNIVERSAL-27-STRUCT-DESTRUCTOR-CONTRACTS.md`
  - Registra causa, cobertura sustituta y limites.

## Verificacion pendiente del commit

- Reconstruir el arbol de pruebas existente.
- Ejecutar las 684 pruebas completas configuradas con O0 y NSLink.
- Confirmar que Stage 40, Stage 56 C6 y T06 partial-moves pasan juntos.
