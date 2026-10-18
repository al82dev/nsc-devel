# Tanda NSC305 GATE-F05A — field-store null-conditional lazy

## Alcance

Esta hoja restaura el contrato de `NullConditionalFieldStore` sin cambiar sintaxis, HIR, MIR,
ABI, NSMETA, diagnósticos ni ownership. El receptor se evalúa una vez; el branch null/no-null se
emite antes de bajar el RHS; la rama nula produce cero/null y no ejecuta efectos del RHS.

`NullConditionalArrayStore` queda fuera de esta hoja y se conserva como GATE-F05B independiente.

## Causa

La migración L04AK apiló receptor y RHS antes de llamar al emisor del store. Aunque el helper
histórico contenía el branch correcto, recibir un `valueOverride` ya emitido hizo eager el RHS.
La suite `Host.Frontend` usada en L04AK no ejecutó los productos externos TL-LANG-340/342/346.

## Diseño

La máquina LLVM usa tres fases:

1. bajar únicamente el receptor;
2. preparar dirección, carga compound y bloques null/value/end, posicionarse en value y apilar el
   RHS;
3. reanudar con el RHS, calcular simple/compound/concat, destruir el `owned string` desplazado,
   almacenar y construir el PHI.

El contexto necesario permanece en `Frame`; ningún helper vuelve a llamar `lowerExpression`, por
lo que no se reintroduce el edge recursivo eliminado por L04.

## Validación requerida

- build focal de `nsc` y `nsc_component_tests`;
- TL-LANG-340: resultado 42, dos receptores y un RHS;
- TL-LANG-342: resultado 15, dos receptores y un RHS;
- TL-LANG-346: resultado 11, dos receptores, dos RHS y ownership sin crash;
- repetición con `--no-artifact-cache` cuando el runner la admita;
- auditoría estática: el push de `children[1]` ocurre después de `LLVMBuildCondBr`, cero llamada
  recursiva desde la ruta field-store y ninguna modificación a GATE-F05B.

No se agrega una prueba C++ que duplique semántica: los tres casos existentes ejercen el
`nsc.exe` real y ya están registrados en manifiesto/catálogo/Builder.

## Evidencia de cierre

- `nsc` PASS en `build-clang` y en el árbol usado por la compiler-suite.
- `nsc_component_tests` PASS al construir el target focal.
- Runner registrado: TL-LANG-340, TL-LANG-342 y TL-LANG-346 PASS, exit 0 del runner.
- Contrafactuales directos O0 con `--no-artifact-cache`: compilación exit 0 y ejecutables exit
  42, 15 y 11 respectivamente; esos exits prueban también los contadores exactos embebidos en los
  fixtures (un RHS para 340/342 y dos para 346).
- La telemetría de los tres builds informó `artifact-cache-hits: 0` y
  `artifact-cache-misses: 0`.
- `validate_source_registration.py` PASS.
- Auditoría estática: cero ocurrencias del helper recursivo anterior, una definición y una llamada
  para cada helper prepare/finish, y cero `lowerExpression` dentro de la familia de helpers F05A.
- `git diff --check` PASS para los archivos propios.

Logs persistentes:

- `A:\temp\gate-f05a-build-nsc.log`
- `A:\temp\gate-f05a-build-test-infra-nsc.log`
- `A:\temp\gate-f05a-build-component-runner.log`
- `A:\temp\gate-f05a-TL-LANG-340.log`
- `A:\temp\gate-f05a-TL-LANG-342.log`
- `A:\temp\gate-f05a-TL-LANG-346.log`
- `A:\temp\gate-f05a-TL340-no-cache.log`
- `A:\temp\gate-f05a-TL342-no-cache.log`
- `A:\temp\gate-f05a-TL346-no-cache.log`
