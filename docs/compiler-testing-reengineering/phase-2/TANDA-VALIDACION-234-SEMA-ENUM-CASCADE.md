# Tanda de validación 234 — cascada en inicializadores enum

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

Cuando un inicializador de `enum` contenía un identificador desconocido,
`analyzeExpression` ya producía `UnknownIdentifier` y tipo `Error`. La ruta de
`analyzeEnum` volvía a clasificar ese sentinel como inicializador no constante
y añadía `InvalidEnumInitializer`, ocultando la causa primaria con ruido
derivado.

## Corrección

La ruta enum reconoce `TypeKind::Error`, conserva el miembro en estado no
válido para recuperación y no agrega un segundo diagnóstico. Los
inicializadores bien tipados pero no constantes continúan produciendo
`InvalidEnumInitializer`.

## Alcance

La causa corresponde a `TS-SEMA-116` y no cambia la evaluación ni el rango de
valores enum.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
