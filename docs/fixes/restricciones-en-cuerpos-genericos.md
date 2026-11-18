# Restricciones anidadas en sobrecargas genéricas

Al crear las fábricas internas de punteros se observó que el compilador
materializaba un tipo restringido dentro de una sobrecarga de clase incluso
cuando se elegía la alternativa para estructuras. El diagnóstico del cuerpo
descartado impedía compilar un programa válido. La reproducción con los
archivos originales de la investigación se conserva en
`A:/nsc-fixes-evidencia/shared-handles-failure`; sus variantes y resultados
están en `shared-overload-delta`.

La corrección conserva el grupo de sobrecargas durante el procesamiento del
cuerpo especializado. La comprobación semántica examina los tipos usados en
los cuerpos que analiza y conserva los argumentos genéricos que la
materialización reescribe. Así también diagnostica un tipo inválido utilizado
por el cuerpo comprobado, incluso si aparece dentro de otro tipo, un arreglo o
los argumentos de una llamada. Se restituyó `where T : class` en el contenedor
interno `SharedClassCreation`, que antes se había retirado como solución
provisional al falso rechazo.

Validación dirigida con el compilador de `A:/nsc-fixes-evidencia/build`:

- `tests/integration/generic_selected_body_constraints.py`: casos válidos y
  restricciones inválidas en locales, construcción, arreglos, tipos anidados y
  llamadas; control de uso directo y ejecución en O0/O2. Resultado: pasa.
- `shared-overload-delta.py`: seis variantes, incluida la reproducción original;
  todas pasan el análisis semántico. `shared-overload-constraint-probe.py`:
  los controles válidos pasan y las seis variantes inválidas son rechazadas con
  el diagnóstico de restricción esperado.
- `nsc.ns3.t04.generics` y `nsc.ns3.t04.declarative-constraints`: 2/2 pasan;
  las cinco pruebas dirigidas de almacenamiento y controles internos de
  punteros también pasan. Logs en `A:/nsc-fixes-evidencia`.

La evidencia cubre las formas enumeradas y las fuentes compiladas en esta
copia. No acredita todos los usos posibles de tipos reescritos, ni módulos
genéricos precompilados o todas las rutas de caché. No se ejecutó batería
completa, conforme a la instrucción de usar pruebas dirigidas.
