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

## Evidencia posterior al commit

- Se reconstruyeron los tres ejecutables afectados en `build-test-nslink`.
- Se ejecutaron las 684 pruebas existentes con O0 y
  `C:\native-sharp\bin\nslink.exe`; no se ejecuto baseline ni instalacion.
- Resultado global: 597 pruebas pasaron y 87 fallaron en 88.99 segundos.
- `nsc.stage40.struct-value-abi` paso.
- `nsc.stage56.c6-struct-members` paso.
- `nsc.ns3.t06.partial-moves` paso, incluida la ejecucion exactamente una vez del destructor.

## Conclusion

Las dos negativas obsoletas desaparecieron sin perder la prueba observable del destructor ni las
fronteras que siguen siendo invalidas. La tanda queda verificada y cerrada.
