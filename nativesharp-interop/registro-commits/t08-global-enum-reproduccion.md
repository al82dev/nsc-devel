# Tanda 08: reproducción de variable global C enum

- Se prepara un global C `enum Mood shared_mood`, se asigna desde NativeSharp y se lee tanto desde NativeSharp como desde C.
- El enum también aparece en otras firmas del proyecto: este caso aísla la proyección y el acceso, pero no demuestra selección del tipo cuando el global es su única referencia.
- Evidencia estática: `mapGlobalType` no proyecta `CppTypeKind::Enumeration`; el proyecto debería llegar al diagnóstico de tipo global no mapeado. Pendiente de confirmar en la siguiente validación agrupada.
