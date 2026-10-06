# SEM-02.03 - Construction consume sitios tipados

## Resultado

`ProjectConstructionModel` dejó de recorrer por segunda vez cada cuerpo, inicializador de campo e
inicializador de constructor. La resolución ahora consume directamente los sitios que publicó
`ProjectMethodBodyModel`.

## Compatibilidad de orden

Los sitios se ordenan explícitamente por:

1. ordinal estable del callable;
2. secuencia postorden dentro del callable.

Esto conserva el contrato histórico: inicializadores de constructor antes del cuerpo,
construcciones anidadas antes de la exterior, miembros por ordinal y funciones libres después de
los miembros. El instante de terminación de análisis no participa en el orden.

## Cambios de costo

- Eliminado `scanNode`, que visitaba todos los nodos de todos los cuerpos después de MethodBody.
- Eliminadas las consultas de tipo por argumento para `new` y placement; el sitio ya transporta
  los bindings completos.
- Se conserva únicamente la validación de contextos declarativos externos a cuerpos. Ese barrido
  es ahora iterativo, con worklist en heap y poda las declaraciones ejecutables.
- Los inicializadores `this(...)`/`base(...)` siguen usando su ruta declarativa especializada;
  no son expresiones de construcción de objetos y no forman parte del segundo recorrido retirado.

## Seguridad

- Un sitio con argumentos incompletos conserva `NoViableCandidate` sin inventar tipos.
- El tipo objetivo procede de la fase semántica anterior y se vuelve a validar como class/struct.
- No se introdujo recursión.

## Validación dirigida

- Compilar y ejecutar sólo `nsc_project_construction_model_tests` en O0.
- Confirmar estáticamente que no quedan `scanNode` ni recorridos de cuerpos en Construction.
- Sin baseline, CTest general, O1, O2 ni O3.
