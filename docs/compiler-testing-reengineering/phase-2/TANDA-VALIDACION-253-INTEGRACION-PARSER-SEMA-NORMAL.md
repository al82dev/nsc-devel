# Validación 253: integración normal de Parser y Sema

## SCOPE LOCK

- Objetivo: integrar únicamente `TP-PAR-514`, `TP-PAR-516`, `TP-PAR-524`,
  `TS-SEMA-101`, `TS-SEMA-265` y `TS-SEMA-281` en el target normal existente.
- Producto protegido: compilador, semántica, API, ABI y construcción de NSC.
- Infraestructura protegida: CMake, Builder3, ejecutores, gates y perfiles.
- Fuera de alcance: casos AST, Source, `TS-SEMA-077`, `TS-SEMA-094` y cualquier
  otro candidato pendiente.
- Validación ejecutada: ninguna, por prohibición expresa.

## Integración

Las seis filas usan el executor `nsc_component_tests` y el perfil `PR`. Los
gates de Host.Parser y Host.Sema ya incluyen ese perfil, por lo que no fue
necesario modificar scripts ni inventarios.

- `TP-PAR-516`, `TS-SEMA-101` y `TS-SEMA-281` son `Required`.
- `TP-PAR-514`, `TP-PAR-524` y `TS-SEMA-265` son `KnownProductDefect`; sus
  cuerpos deben fallar al reproducir el defecto para que el controlador emita
  `KNOWN_FAILURE`.
- `TS-SEMA-265` conserva `process-isolation` y usa timeout `short` para confinar
  la falta de terminación del producto.

El manifiesto pasa de 5,695 a 5,701 casos ejecutables y de 42 a 45 casos
`KNOWN_FAILURE`. El inventario histórico de 3,438 candidatos no se reescribe:
la salida de `InfrastructurePending` queda expresada en los registros vigentes
de Parser/Sema y en estas filas del manifiesto.
