# Item 399 — Encapsulación del backend LLVM

## Resultado

- El lowering de proyectos LLVM tiene un único punto de entrada basado en `ProjectOopLlvmLoweringRequest`.
- Las modalidades sin debug, con debug y particionada se construyen mediante fábricas explícitas.
- La frontera valida modelos, contexto de debug y nombre estable de partición antes de crear estado LLVM.
- El driver y el generador de objetos usan la nueva frontera; las firmas anteriores quedan como adaptadores de compatibilidad.

## Validación

- Construcción limpia e instalación del compilador.
- Prueba dedicada `nsc.item399.llvm-backend-encapsulation`.
- Regresiones de lowering, generación de objetos y particionado.
