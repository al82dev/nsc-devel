# Tanda de validación 177 — `braceShieldContract`

## Alcance

- Componente: `Host.LanguageService`.
- Contrato: `CF-LANGUAGE-SERVICE-016`.
- Caso afectado: `TF-LANGUAGE-SERVICE-125`.
- Archivo: `tests/compiler-suite/host/modules/LanguageServiceDocuments081To160Module.cpp`.

## Causa

La variante que conserva una llave dentro del escudo léxico verificaba únicamente
la línea normal `return;`. El servicio entrega correctamente el texto protegido
`{ */ return;`, pero el oráculo descartaba esa diferencia intencional.

## Corrección

El resultado esperado ahora se selecciona explícitamente por variante. La prueba
sigue exigiendo la edición y conserva una comparación dirigida del contenido;
no se relajó el contrato ni se modificó el producto.

## Estado

Implementado. **PENDIENTE DE VALIDACIÓN FINAL** junto con el resto de la tanda;
no se compiló ni se ejecutó ninguna prueba durante este cambio.
