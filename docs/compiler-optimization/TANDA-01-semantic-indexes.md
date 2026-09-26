# Tanda 1: indices semánticos persistentes

## Objetivo

Eliminar recorridos lineales repetidos al buscar tipos y miembros durante el análisis semántico.

## Cambios realizados

- Se añadió un índice persistente de tipos por clave canónica.
- Se añadió un índice de miembros por tipo y nombre.
- `findMembers` ahora devuelve una vista `std::span` sobre el índice, sin construir un vector nuevo en cada consulta.
- Se conservó el orden semántico anterior: miembros físicos primero y miembros promovidos después.
- Se excluyeron de forma explícita los campos portadores anónimos, igual que en la implementación previa.
- Los consumidores que realmente necesitan modificar una lista realizan una copia explícita.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`

## Archivos nuevos

- Este informe. No se crearon archivos de código nuevos porque los índices pertenecen naturalmente al modelo semántico existente.

## Mejora esperada

Las búsquedas de tipos y miembros pasan de recorrer colecciones completas repetidamente a búsquedas de costo promedio constante. Esto beneficia especialmente proyectos grandes y bibliotecas con muchos tipos, sobrecargas y miembros promovidos.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.project-member-model`: correcta.
- Prueba dirigida `nsc.item326.anonymous-member-promotion`: correcta con el runtime O0 de la construcción.
- No se ejecutó baseline completo.
