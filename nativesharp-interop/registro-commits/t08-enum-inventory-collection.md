# Tanda 08: recolección de enums por proyecto

`collectCppSemanticEnums` recoge los snapshots de enum de cada sesión y conserva
su origen dentro del grafo de proyectos. Una sesión ausente o sin snapshot se
devuelve como error con su referencia. No fusiona observaciones ni publica sus
valores como constantes NativeSharp. Pendiente de validación agrupada.
