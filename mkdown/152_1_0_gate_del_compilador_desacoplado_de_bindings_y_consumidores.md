# 152 — Gate 1.0 del compilador desacoplado de bindings y consumidores

**Fecha:** 2026-09-08  
**Afecta:** alcance de la promoción `nsc 1.0.0`  
**Archivo normativo de planificación:** `121_next_steps_for_the_1_0_glory.md`

## 1. Decisión

La promoción del compilador a `1.0.0` no depende de una demo, de un juego ni de que un catálogo de
bindings alcance una cobertura determinada.

Los bindings permanecen en desarrollo y tienen autoridades, repositorios y calendarios propios.
Convertir uno de sus consumidores en gate del compilador mezclaría dos estados independientes:

- la corrección del lenguaje, backend, linker, ABI y toolchain;
- la amplitud y madurez de paquetes externos todavía cambiantes.

Un consumidor real sigue siendo evidencia útil para encontrar un defecto. No se convierte por eso
en parte del producto, en fixture obligatoria ni en requisito para asignar la versión del compilador.

## 2. Qué certifica ahora NativeSharp 1.0

El gate queda limitado a propiedades bajo control de este repositorio:

1. construcción e instalación limpias;
2. semántica equivalente en O0 y O2;
3. matrices completas con lld-link y NSLink;
4. layout y ABI contrastados con clang-cl/MSVC;
5. ownership y destrucción determinista sin defectos conocidos en el corpus;
6. `.lib` y DLL NativeSharp consumibles mediante sus contratos públicos;
7. metadata, ABI, runtime y formatos de paquete versionados;
8. binarios reproducibles y paquetes autenticados;
9. diagnósticos y errores internos con contratos verificables;
10. documentación pública suficiente para usar y distribuir el toolchain.

Todas estas propiedades pueden tener una fuente de verdad, una fixture autocontenida y evidencia
repetible dentro del compilador.

## 3. Qué queda fuera del gate

No se exige:

- una aplicación completa construida sobre bindings en desarrollo;
- una ventana, audio, input, assets o un loop integrados como prueba de promoción;
- cobertura total de un catálogo externo;
- declarar completo un paquete porque el compilador puede expresar su ABI;
- modificar consumidores externos para acomodar el calendario de `nsc`.

Esto no reduce el contrato ABI. Las fixtures mínimas siguen probando structs, enums, callbacks,
function pointers, strings, buffers, handles y vtables en ambas direcciones.

## 4. Cambios realizados en el roadmap

`121_next_steps_for_the_1_0_glory.md` quedó ajustado en los puntos activos:

- el objetivo habla del toolchain nativo y no de una aplicación concreta;
- la promesa 1.0 se expresa como capacidad de producir software nativo;
- 0.9 beta se define por congelación y consumidores externos, no por un piloto obligatorio;
- el gate 0.85 mide ABI de plataforma y no amplitud de bindings;
- el catálogo de bindings queda explícitamente separado del gate del compilador;
- el gate 0.85 termina en fixtures ABI autocontenidas;
- el soak final se ejecuta sobre el corpus del compilador;
- la documentación final exige una guía de distribución del toolchain;
- el orden de estabilización responde a bugs y evidencia del corpus.

## 5. Qué no se modificó

- No se borraron ejemplos existentes.
- No se alteraron repositorios ni código de bindings.
- No se reescribieron actas históricas que registran consumidores ejecutados por corridas anteriores.
- No se cambió código del compilador.
- No se cambió el baseline Stage 66 certificado por G00.

Las actas permanecen fieles a lo que realmente ocurrió. Este documento cambia el criterio futuro,
no la evidencia pasada.

## 6. Nuevo cierre pendiente para 1.0

Después de Stage 66, el trabajo restante se concentra en una megaentrega de estabilización:

- soak reproducible del corpus en los cuatro cuadrantes;
- estrés explícito de ownership, excepciones y fronteras entre módulos;
- consolidación del manual, guía de ownership, guía FFI y distribución;
- lista pública y corta de limitaciones aceptadas;
- periodo sin cambios semánticos;
- gate `release-ready` que reúna las autoridades ya certificadas;
- promoción final del número a `1.0.0` mediante un nuevo G00.

El estado de los bindings no bloquea ninguno de esos puntos.
