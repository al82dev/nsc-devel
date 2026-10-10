# Tanda de validación 151 — Productores self-host contenidos

## Estado

`IMPLEMENTADA; VALIDACIÓN INTEGRAL DIFERIDA HASTA VALIDACIÓN 155`

## Defecto concreto

Cinco productores self-host entregaban a NSC proyectos cuyos elementos
`Compile` contenían segmentos `../../..`. El cargador de proyectos rechaza
correctamente esos patrones porque un patrón de compilación debe permanecer
dentro del directorio del proyecto. Los cinco productores fallaban antes de
generar su ejecutable y sus 187 consumidores quedaban bloqueados.

## Causa

Los fixtures intentaban mezclar en un solo proyecto fuentes locales de prueba
y fuentes de `selfhost/nsc` ubicadas fuera del directorio del proyecto. La
infraestructura asumía que `Compile Include` aceptaba navegación al padre; ese
supuesto contradice el contrato vigente de globbing de proyectos.

## Corrección mínima

`materialize_contained_project` crea dentro del directorio único de la corrida
una copia efímera del proyecto:

- resuelve cada fuente explícita contra el proyecto canónico;
- rechaza escapes del repositorio, comodines, ausencias y duplicados;
- copia exactamente los bytes bajo `project/sources/...`;
- reescribe los `Compile` como rutas locales sin `..`;
- reescribe `ProjectReference` como ruta absoluta validada dentro del mismo
  repositorio;
- no modifica los cinco `.nsproj` canónicos ni una fuente de producto;
- rechaza reutilizar un destino existente para impedir consumo de residuos.

Los cinco gates — Basic, Diagnostics, Lexer, Source y Types — compilan sólo el
proyecto materializado de su propia corrida.

## Contrato preservado

Las fuentes, referencias, casos, argumentos, optimización, perfil, oráculos y
resultados esperados no cambian. La transformación afecta únicamente la forma
de presentar al cargador el mismo conjunto exacto de archivos.

## Evidencia preparada

La prueba dirigida nueva comprueba los cinco proyectos vivos, la ausencia de
segmentos padre, el confinamiento de cada copia, la identidad byte a byte, las
referencias existentes y el rechazo de destino reutilizado o proyecto externo.

Por instrucción expresa no se ejecutó todavía ninguna prueba. Esta tanda se
validará junto con las otras cuatro correcciones en la corrida única final
`Debug/O0/Todos`.
