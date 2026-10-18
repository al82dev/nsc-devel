# Correctiva LLVM: operandos MIR ausentes en el plan de partición

## Alcance

Esta correctiva se limita al planeamiento de unidades LLVM. No modifica el modelo MIR, el lowering
LLVM, la sintaxis, ABI, NSMETA, diagnósticos ni el formato de artefactos. Los `nullptr` almacenados
en `ProjectOopMirNode::children` representan operandos estructuralmente ausentes y válidos; por
ejemplo, los huecos opcionales de una construcción `for`.

## Causa demostrada

El caso NeverType de NS3-T07 terminaba con `0xC0000005` antes de crear el módulo LLVM. Una traza
temporal, sobreviviente al fallo y retirada antes de esta correctiva, dejó como última frontera
`scan.callee.enter function=10` sin dirección de nodo. El worklist de
`projectOopReachableDefinitionMask` había extraído un puntero nulo que se había insertado desde un
`unique_ptr` hijo vacío y lo desreferenciaba inmediatamente mediante `node->calleeName`.

La ejecución con `--no-artifact-cache` reprodujo el mismo acceso, por lo que restauración, clave y
contenido de caché quedaron descartados como causa. La instrumentación temporal dejó diff cero
antes de aplicar la corrección productiva.

## Corrección

Los cinco recorridos iterativos que participan en el plan de partición ahora:

1. descartan defensivamente un puntero nulo inmediatamente después de extraerlo del worklist; y
2. no insertan hijos nulos en el worklist.

Las cinco rutas auditadas son:

- `projectOopReachableDefinitionMask`;
- `projectOopStaticFieldDeclarationMask`;
- `projectOopCoordinatorFunctionDeclarations`;
- `nodeCost`; y
- el recolector de dependencias `collectNode` de `ProjectOopCodePartition::build`.

El orden LIFO de los nodos presentes, los cálculos de coste, el descubrimiento de dependencias y la
selección de declaraciones permanecen idénticos. Un operando ausente no aporta coste, dependencia
ni declaración, que es el contrato semántico ya aplicado por `appendNodeSourceRanges`.

## Validación focal

- Build `nsc`: PASS. Log: `A:\temp\never-null-partition-nsc-build.log`.
- NeverType O0, `-j1`, `--no-artifact-cache`: PASS, exit 0, 449 ms. Log:
  `A:\temp\never-null-partition-no-cache-j1.log`.
- NeverType O0, `-j1`, caché habilitada: PASS, exit 0, 426 ms. Log:
  `A:\temp\never-null-partition-cache-j1.log`.
- Build `nsc_ns3_t07_error_tests`: PASS. Log:
  `A:\temp\never-null-partition-ns3-t07-build.log`.
- CTest `nsc.ns3.t07.errors`: el proceso completó sin `0xC0000005` y pasó 57/58 checks. El único
  fallo restante ocurre después de NeverType y corresponde al round-trip NSMETA 1.38 de typed
  throws. Se registra como `NSMETA-F01` y no se mezcla con esta correctiva. Log:
  `A:\temp\never-null-partition-ns3-t07-ctest.log`.

## Auditoría estática

Se revisaron todos los `push_back(...get())` de `ProjectOopFunctionReachability.cpp` y
`ProjectOopCodePartition.cpp`. Los cinco worklists del planificador filtran hijos nulos. El recorrido
análogo `appendNodeSourceRanges` de `ProjectOopObjectGenerator.cpp` ya tenía guard por iteración y no
requirió modificación. `git diff --check` no reporta errores.
