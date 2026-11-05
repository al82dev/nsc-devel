# Tanda 08: proyección inicial de arreglos globales C

- `mapGlobalType` prepara la proyección de arreglos fijos de elementos escalares representables, conservando dimensiones en el orden de identidad de NSC y el tipo C canónico.
- El helper de aliases rechaza límites incompletos y calificadores de arreglo no representados; se rechazan además elementos const o volatile. No se implementan todavía arreglos globales de enums o records.
- Pendiente de validación en un proyecto mixto con arreglo y matriz; la proyección aislada no cierra T08-004 ni T08-016.
