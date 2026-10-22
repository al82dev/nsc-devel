# NSC — Pendientes y terminados de rendimiento

**Progreso:** `46 / 48`

| Ítem | Qué resuelve | Estado |
|---|---|---|
| T1.1 | Conectar `setTemplateCacheRoot` desde la entrada real de compilación | TERMINADO |
| T1.2 | Usar una raíz persistente independiente del directorio temporal del build | TERMINADO |
| T1.3 | Calcular la llave de plantillas antes del lexer y parser | TERMINADO |
| T1.4 | Restaurar el producto parseado con su identidad y orden originales | TERMINADO |
| T1.5 | Omitir lexer, parser y recolección de templates cuando exista un hit válido | TERMINADO |
| T1.6 | Publicar el producto solamente después de procesarlo correctamente | TERMINADO |
| T1.7 | Conservar publicación atómica, cuarentena, poda y razones de hit o miss | TERMINADO |
| T2.1 | Conectar `setMirCacheRoot` desde la entrada real de compilación | TERMINADO |
| T2.2 | Calcular la llave MIR antes del lowering | TERMINADO |
| T2.3 | Incluir HIR, tipos, ABI, compilador, target y opciones en la llave MIR | TERMINADO |
| T2.4 | Consultar la caché antes de construir cada callable MIR | TERMINADO |
| T2.5 | Instalar la función MIR restaurada en vez de descartarla | TERMINADO |
| T2.6 | Reenlazar referencias, cleanups, excepciones, async frames y ordinales restaurados | TERMINADO |
| T2.7 | Construir y publicar solamente los misses MIR | TERMINADO |
| T3.1 | Definir la identidad estable de cada módulo precompilado | TERMINADO |
| T3.2 | Crear una raíz persistente para productos de módulos precompilados | TERMINADO |
| T3.3 | Restaurar `public-api` sin volver a generarlo | TERMINADO |
| T3.4 | Restaurar `public-templates` sin volver a generarlo | TERMINADO |
| T3.5 | Reconstruir las unidades virtuales con su identidad y orden originales | TERMINADO |
| T3.6 | Omitir emitter, lexer, parser y genéricos de la superficie restaurada | TERMINADO |
| T3.7 | Publicar e invalidar ambos productos como una unidad atómica | TERMINADO |
| T4.1 | Crear un repositorio de NSMETA por compilación | TERMINADO |
| T4.2 | Leer y calcular una sola vez el hash de cada NSMETA sin cambios | TERMINADO |
| T4.3 | Decodificar una sola vez cada payload NSMETA | TERMINADO |
| T4.4 | Ejecutar una sola vez `verifyCurrentCompiler` por identidad NSMETA | TERMINADO |
| T4.5 | Compartir el mismo modelo NSMETA entre todos sus consumidores | TERMINADO |
| T4.6 | Persistir y restaurar el modelo NSMETA verificado | TERMINADO |
| T4.7 | Invalidar NSMETA por contenido o contrato sin cambiar diagnósticos | TERMINADO |
| T5.1 | Asignar un propietario único a los índices reutilizables | TERMINADO |
| T5.2 | Compartir el índice de tipos por identidad, nombre y ordinal | TERMINADO |
| T5.3 | Compartir índices de miembros, firmas y dispatch | TERMINADO |
| T5.4 | Compartir índices de layouts de structs, clases y campos | TERMINADO |
| T5.5 | Compartir índices de templates, demandas y especializaciones | TERMINADO |
| T5.6 | Compartir índices HIR de locales, lambdas y cleanups | TERMINADO |
| T5.7 | Compartir índices MIR de funciones, miembros y async frames | TERMINADO |
| T6.1 | Construir el manifiesto de entradas antes de `ProjectCompilation` | TERMINADO |
| T6.2 | Incluir todas las entradas relevantes en la identidad de compilación | TERMINADO |
| T6.3 | Consultar el bundle existente antes de construir el frontend | TERMINADO |
| T6.4 | Verificar todos los artefactos solicitados antes de aceptar un hit | TERMINADO |
| T6.5 | Restaurar artefactos válidos y omitir las fases ya resueltas | TERMINADO |
| T6.6 | Publicar el bundle solamente después de una compilación exitosa | TERMINADO |
| T6.7 | Registrar invalidación, corrupción y razón exacta de miss | TERMINADO |
| T7.1 | Crear un propietario de vida de compilación para pool, scheduler, cachés e índices | TERMINADO |
| T7.2 | Compartir el mismo pool entre grafo, sintaxis, nombres, HIR, MIR y LLVM | TERMINADO |
| T7.3 | Eliminar pools internos redundantes por fase | TERMINADO |
| T7.4 | Reutilizar productos inmutables sin copiarlos ni reconstruirlos | TERMINADO |
| T7.5 | Conservar orden, barreras, cancelación y estado floating-point deterministas | PENDIENTE |
| T7.6 | Reiniciar el estado al terminar sin contaminar compilaciones posteriores | PENDIENTE |
