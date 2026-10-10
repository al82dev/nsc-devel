# Tanda Validacion 131 - metadato estable de variantes de Passes

## Alcance

Esta tanda corrige exclusivamente la perdida de `layer` durante la
materializacion iterativa de los contratos `Host.Passes`. No modifica el
producto, el registro comun, CMake, manifiestos, casos, IDs, contratos,
oraculos, disposiciones ni la bateria historica.

## Evidencia de entrada

- La configuracion Debug de la suite nueva termino correctamente en `A:`.
- La construccion de `nsc_component_tests` termino `536/536`.
- Los 116 gates CTest fallaban antes de ejecutar casos porque
  `nsc_component_tests --list` terminaba con
  `test case metadata is incomplete`.
- `TestRegistry::add` rechaza de forma determinista cualquier `layer` vacio.

## Auditoria causal completa

Se revisaron los catorce helpers que expanden un contrato en variantes:

1. `CfgTestSupport.h`
2. `CompilationTestSupport.h`
3. `DominanceTestSupport.h`
4. `DriverTestSupport.h`
5. `FrontendContractSupport.h`
6. `HIRContractSupport.h`
7. `InteropTestSupport.h`
8. `LanguageServiceTestSupport.h`
9. `LlvmTestSupport.h`
10. `MirContractSupport.h`
11. `NscTestSupport.h`
12. `PassTestSupport.h`
13. `RuntimeTestSupport.h`
14. `SsaContractSupport.h`

Trece conservaban la capa durante las ocho inserciones. Solamente
`addPassContract` usaba `std::move(layer)` dentro del bucle. La variante uno
consumia la cadena y la variante dos entregaba una cadena vacia al registro.

La cadena causal comprobada es:

```text
contrato Host.Passes
-> variante 1 mueve layer
-> variante 2 recibe layer vacio
-> TestRegistry::add viola el contrato de metadato completo
-> el productor --list termina antes de enumerar
-> los 116 gates quedan bloqueados por el mismo productor
```

## Correccion

`addPassContract` copia `layer` en cada `TestCase`, igual que los otros trece
productores iterativos. No cambia la identidad, el orden, la disposicion ni el
cuerpo ejecutable de ninguna variante.

## Validacion ejecutada

| Gate | Resultado |
|---|---:|
| Construccion incremental del target | `8/8`, PASS |
| `nsc_component_tests --list` | salida `0` |
| Filas enumeradas por el productor | `8,054` |
| Primeras filas | infraestructura presente |
| Ultimas filas | `SelfHost.Syntax` presente |
| Archivos protegidos | seis hashes sin cambios |

La validacion inicial de los gates 1--4 encontro despues un defecto distinto:
el inventario invoca `--list` con un limite de 60 segundos, mientras el
registro tarda mas porque ordena todo el vector despues de cada insercion. Ese
defecto no pertenece a esta hipotesis causal y queda como siguiente tanda. El
gate integral `--all` iniciado por esa validacion conserva su ejecucion para
recoger evidencia funcional; no se usa para ampliar este commit.

## Estado

La perdida de metadato de `Host.Passes` queda corregida y el productor comun
vuelve a materializar el inventario completo. La tarea persistente continua
activa hasta que los 116 gates de la suite nueva terminen sin errores
inesperados. No se ejecuto baseline, no se desplego y no se escribio en `C:`.
