# Callback C almacenado con enum en la firma

El fixture prepara un callback que recibe y devuelve `enum Mood`, almacenado
en un campo y un global C. NativeSharp lo lee e invoca y después asigna un
callback sin capturas al almacenamiento para que C lo invoque. La prueba no
usa operaciones mixtas enum/entero, pendientes de decisión por separado.
Con NSC build `3f82235fd35e7b9a`, el campo se rechazó como miembro no
compatible y el global como tipo no mapeado. El mapeador de callbacks
almacenados contiene un rechazo explícito de enum; la función existente
`projectCppSemanticEnumType` comprueba identidad, origen, representación,
proyecto y conflictos usando el lookup. Se prepara usar esa ruta para el
callback almacenado, sin cambiar la semántica de operaciones mixtas.

Se retiró el rechazo específico del mapeador de callbacks almacenados para
que use `projectCppSemanticEnumType` con el proyecto y lookup disponibles. Se
preparó un control positivo de identidad y uno con USR incompatible, además
de la ejecución del fixture de campo y global en O0–O3. El cambio está sin
construir ni validar; no cubre todas las firmas, representaciones ni metadata.
