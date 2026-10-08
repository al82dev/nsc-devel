# Tanda universal 50: clasificación física residual en el self-host

## SCOPE LOCK

- Objetivo exacto: eliminar las comparaciones directas restantes que distinguen incorrectamente
  `owned X` o `borrowed X` de su tipo físico base dentro del backend self-hosted.
- Naturaleza: cierre correctivo de la causa raíz documentada en la tanda 49.
- Comportamiento protegido: no cambia la sintaxis, semántica de ownership, ABI genérico,
  representación de referencias, pruebas, fixtures ni diagnósticos.
- Revisión conocida como correcta: `nsc0` y los contratos vigentes del backend.
- Rango causal examinado: estado vigente después de `f0fad42ef`.
- Archivos autorizados: `selfhost/nsc/CodeGen/LlvmBackend.ns` y este documento.
- Archivos protegidos: parser, semántica, runtime, bibliotecas, fixtures, verificadores, NSLink,
  metadatos y cualquier contenido en `C:`.
- Pruebas protegidas: todas las pruebas existentes deben permanecer sin cambios.
- Acciones prohibidas: build, pruebas locales, baseline, O1/O2/O3, deploy, cambios en `C:`,
  workarounds en consumidores y recursión nueva.
- Criterio de terminación: inventario de comparaciones físicas auditado, usos residuales migrados
  a la clasificación normalizada, documentación y código incluidos en un commit local.

## Inventario residual

La búsqueda estática posterior a la tanda 49 encontró comparaciones exactas de nombres semánticos
en estas rutas físicas:

1. inicializadores constantes booleanos;
2. lectura y escritura de bitfields booleanos;
3. promoción vararg de `float`;
4. condiciones booleanas constantes y dinámicas;
5. acceso a datos y longitud de `string`.

Los clasificadores de enteros, flotantes y signedness ya normalizan los calificadores. Las listas
de placeholders genéricos no forman parte de este inventario: allí `owned T` es información
semántica necesaria para reconocer el ABI borrado y no debe convertirse prematuramente en `T`.

## Corrección

`PhysicalTypeIs` concentra la comparación contra el nombre físico iterativamente normalizado. Las
rutas de `string` reutilizan `StringType`, que además preserva la clasificación por
`SemanticTypeKind.StringLiteral`. Esto evita que cada consumidor mantenga su propia lista de
variantes calificadas.

## Complejidad

Cada clasificación recorre como máximo los prefijos de ownership y el nombre del tipo: tiempo
`O(Q + N)` y memoria `O(N)`. No existe recursión nueva.

## Estado

- Inventario estático: completo para comparaciones exactas de escalares y `string` en
  `LlvmBackend.ns`.
- Implementación: aplicada.
- Validación local: no ejecutada por instrucción expresa.
- Validación externa: pendiente.
