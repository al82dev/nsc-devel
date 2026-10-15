# NS304-T17-F05 — Extensions + partial + comptime + metadata separada

- **Estado**: COMPLETO — la combinación extension + partial + comptime verificada.
- **Rama**: development

## Verificado O0

- `TL-LANG-516` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `partial class Foo` con
  método parcial, `extension Foo { this.M()*2 }` y `#run(21)` → exit 42.

## Cobertura de las cuatro aristas

| arista | evidencia |
|---|---|
| extensions | `extension Foo { public int Twice() { return this.M() * 2; } }` |
| partial | `partial class Foo` + `public partial int M();` / cuerpo |
| comptime | `#run(21)` dentro del cuerpo parcial |
| metadata separada | producer/consumer decimal sin fuentes (`TL-LANG-508`) |

## Contexto

- El hueco era probar la combinación. Las aristas ya tenían pruebas individuales (extensiones
  `TL-LANG-512`, partial `TL-LANG-416/417`, comptime `TL-LANG-500..502`, metadata `TL-LANG-508`); aquí
  se cerró la combinación productiva end-to-end. La extensión accede al receptor con `this.M()`.
