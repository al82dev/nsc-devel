# Contrato propuesto: consultas estáticas de campos

Estos fixtures son pruebas pendientes para la propuesta `static foreach
(field in Fields(T))`. Sus fuentes no describen sintaxis ya implementada ni se
registran en CMake. Deben copiarse a una carpeta de evidencia en A antes de
generar proyectos o binarios; no sustituyen el ejemplo 74 original.

- `heterogeneous.ns`: dos tipos de campo, consulta de `Name` y `Type`, acceso
  tipado, especialización explícita `Identity<field.Type>` y selección de
  sobrecarga; espera retorno 0 y que no se incluya el campo estático.
- `empty.ns`: tipo sin campos de instancia; espera cero iteraciones.
- `inherited_visibility.ns`: dos campos homónimos de base y derivada con
  identidad distinta y orden base-derivada; excluye un privado inaccesible.
- `receiver_once.ns`: dos llamadas a `Get` con receptor que tiene efecto
  lateral; espera una evaluación por llamada y dos evaluaciones en total.
- `binary/Producer.ns` y `binary/Consumer.ns`: la misma consulta sobre un tipo
  proveniente de biblioteca binaria; requiere generar `.lib` y `.nsmeta` en
  evidencia y referenciarlos desde un proyecto consumidor aislado.
- `sibling_scopes.ns`: dos bucles hermanos usan el mismo descriptor sobre
  tipos distintos; espera que sus proyecciones no se mezclen.
- `invalid_descriptor_scope.ns`: uso de `field.Type` fuera de su ámbito;
  espera diagnóstico, pendiente de precisar según la implementación.
- `existing_type_name.ns`: existe un tipo `Field.Type` antes y después de un
  bucle cuyo descriptor se llama `Field`; no debe perder su identidad.
- `invalid_empty_descriptor.ns`: operación de descriptor desconocida dentro
  de una iteración sin campos; la ausencia de iteraciones no debe ocultarla.
- `class_simple.ns`, `private_inside.ns`, `private_outside.ns` y
  `protected_outside.ns`: clases fuente y diferentes contextos de acceso.
- `internal_same_project.ns`: visibilidad internal dentro del mismo proyecto.
- `protected_owner.ns`, `protected_derived_this.ns`, `protected_derived_receiver.ns`,
  `protected_base_receiver_invalid.ns` y `protected_unrelated_excluded.ns`:
  selección y acceso protected desde derivada y desde código ajeno. El acceso
  a un receptor de tipo base desde derivada debe rechazarse; fuera de la
  jerarquía no debe exponerse el campo protected.
- `binary/InternalProducer.ns` y `binary/InternalConsumer.ns`: control
  pendiente para un campo internal de otro módulo y uno público visible.
- `compound_field.ns`, `generic_struct.ns` y `partial_struct.ns`: campo
  compuesto, struct genérico y declaraciones parciales, respectivamente.
- `descriptor_shadow.ns`: una declaración local oculta el descriptor; debe
  rechazarse sin producir una expansión parcial.

Las expectativas son el contrato propuesto por el frente de introspección;
son pendientes hasta que exista implementación y se ejecuten. El paso del
receptor mediante `field.Get`, la metadata binaria y los miembros privados
de la consulta todavía deben verificarse en la implementación nueva.

Con el compilador de este checkout se comprobaron por separado las operaciones
de apoyo, sin usar la consulta propuesta: herencia y ocultación de nombres,
paso de clase `owned` a función genérica, sobrecargas, `Identity<int/long>`,
receptor con efecto lateral y acceso normal a una biblioteca binaria. Los
controles O0 compilaron y ejecutaron con retorno 0; comandos, fuentes e imports
están en `A:/nsc-limitaciones-campos-evidencia/static-field-controls/`.
Esto no valida la sintaxis ni la semántica de la consulta de campos.

Controles adicionales pendientes de definir y ejecutar junto con la expansión:
descriptor con nombre idéntico al de un genérico exterior; iteraciones anidadas
con descriptores homónimos; rechazo de usos no soportados del descriptor;
`field.Type` anidado dentro de otro tipo genérico; y conservación de préstamos
y solo lectura al usar `field.Get`. Estas filas no están cubiertas por los
fixtures anteriores y no deben contarse como aprobadas.
Se decidió rechazar `break` y `continue` cuyo destino sea la iteración estática
o un bucle exterior, preservando los saltos a bucles internos. Una sonda O0
pasó cinco controles de esos casos, incluidos tipos vacíos; comandos y evidencia
en `A:/nsc-limitaciones-campos-evidencia/static-field-control-flow/results.json`.
Los saltos etiquetados y la anidación más amplia siguen pendientes.

El tipo real `Field.Type` se comprobó por separado sin consulta estática: la
fuente en `A:/nsc-limitaciones-campos-evidencia/static-field-controls/existing-type`
compiló y ejecutó con retorno 0; eso no valida la coexistencia del descriptor.

Los fixtures nuevos de clases, visibilidad, campo compuesto, genérico y
partial no se han ejecutado todavía. Cada rechazo deliberado se clasificará
como provisional y no como caso resuelto; `protected_outside` debe rechazarse
de manera provisional hasta resolver completamente la accesibilidad.
