# Tanda de Validación 62: cierre estático de Builder 3

Fecha: 10 de octubre de 2026
Estado: CIERRE ESTÁTICO; VALIDACIÓN FUNCIONAL EXTERNA PENDIENTE

## Universo auditado

Se realizó un inventario mecánico completo de `nscbuilder3.py` después de las
Validaciones 28–61:

- propiedad, cancelación y timeout de procesos;
- conversiones `bool`, `int` y `str`;
- productores y consumidores de estados terminales;
- publicación durable de configuración y evidencia;
- manifiesto ejecutable, inventarios CSV y catálogos de disposición;
- cardinalidad e identidad de casos y variantes;
- construcción no operativa de la GUI.

## Resultado

No queda un defecto estático demostrado en esas fronteras:

- las dos llamadas `Popen` pertenecen a `ProcessController`;
- las dos llamadas directas `subprocess.run` son `git rev-parse` de sólo lectura
  y `taskkill` para terminar el hijo ya poseído;
- los gates importados sustituyen su `run_process` por el runner cancelable;
- conversiones numéricas de entrada están precedidas por validación de forma;
- las conversiones restantes son serialización de rutas, argumentos,
  diagnósticos o estado ya validado;
- configuración y artefactos terminales se publican atómicamente;
- el cargador canónico valida las `5695` filas ejecutables;
- `9587` casos, `44` componentes y `1208` contratos quedan clasificados;
- `12542/12542` variantes poseen clave única;
- casos `UNCLASSIFIED`: `0`.

## Evidencia ejecutada

- `python nscbuilder3.py --audit-catalog`: PASS.
- `python nscbuilder3.py --gui-smoke`: PASS.
- Suite Python completa del controlador: `199/199` PASS.
- Seis archivos protegidos: hashes originales preservados.

## Frontera de la afirmación

Este cierre no compiló NSC, no ejecutó `nsc_component_tests`, no ejecutó gates
reales ni casos NativeSharp y no acredita `12542/12542` resultados funcionales.
La tarea permanece activa. El siguiente insumo es la corrida funcional que el
usuario realiza fuera de esta instancia; cada error real de esa corrida se
incorporará al mismo ciclo causal hasta llegar a cero.
