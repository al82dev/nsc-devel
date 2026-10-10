# Validación 149 — cierre de gates e identidad en Builder 3

## Alcance

Esta tanda corrige exclusivamente dos fronteras confirmadas de Builder 3 en la
corrida `20261010-090333-82a4a13f`. No modifica NSC, contratos de producto,
oráculos C++, manifiestos ni expectativas.

## Causas cerradas

### Carga de gates Python

`ExecutionEngine._load_gate()` ejecutaba cada gate mediante una ruta absoluta,
pero esa operación no hacía importable su directorio hermano. Los gates que
importan `component_gate_support` terminaban antes de ejecutar el contrato.

Builder 3 publica ahora una sola vez la raíz canónica
`tests/compiler-suite/controller/integration` en `sys.path`. La ruta se mantiene
para imports diferidos y la caché de módulos sigue evitando cargas repetidas.

### Contadores de identidad

El controlador canónico publica todos los estados de `CaseStatus` en `counts`,
incluido `SKIPPED: 0`. Builder 3 comparaba ese mapa contra un vocabulario que no
contenía `SKIPPED` y rechazaba cuatro reportes coherentes.

`SKIPPED` se admite únicamente en el mapa de contadores del reporte enfocado.
No se acepta como resultado de la prueba seleccionada y un contador distinto de
cero continúa siendo una contradicción.

## Pruebas dirigidas

- carga de un gate sintético que importa un módulo hermano;
- reutilización exacta del módulo desde la caché;
- publicación única de la raíz de imports;
- aceptación de `SKIPPED: 0`;
- rechazo de `SKIPPED: 1` y de un resultado terminal `SKIPPED`.

## Resultado esperado en la próxima ejecución

- `36` casos BuildSystem dejan de fallar antes de entrar al gate;
- `4` casos de identidad dejan de rechazar un sobre canónico válido.

Ese resultado no se promueve a validación funcional hasta ejecutar los casos
afectados con el binario y la configuración seleccionados por el usuario.
