# Item 398 — Genéricos e interfaces consolidados en MIR

## Resultado

- MIR publica un catálogo único por identidad de especialización genérica y relaciona sus funciones, campos estáticos y tablas de despacho.
- MIR publica enlaces completos clase/interfaz con identidad estable, miembros de contrato, implementaciones, símbolos y slots.
- El backend resuelve el despacho de cleanup de interfaz desde MIR, sin reconstruirlo desde modelos semánticos.
- El esquema MIR avanza a `1.11` y su volcado expone ambos catálogos.

## Validación

- Construcción limpia e instalación del compilador.
- Prueba dedicada `nsc.item398.mir-generics-interfaces`.
- Regresiones de genéricos, interfaces, ownership y contrato del toolchain.
