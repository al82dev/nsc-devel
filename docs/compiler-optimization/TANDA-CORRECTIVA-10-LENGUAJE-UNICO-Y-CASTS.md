# Tanda correctiva 10: lenguaje unico y casts explicitos

## Proposito

Retirar del compilador las modalidades globales introducidas por ediciones
internas y sustituir la autorizacion difusa de `unsafe` por operaciones de cast
que indiquen exactamente la intencion del programador.

## Contrato autorizado

- NativeSharp tiene una sola semantica acumulativa.
- `<Language>` no selecciona reglas del parser, semantica, ownership, metadata,
  ABI ni backend.
- NSMETA no publica una edicion del lenguaje. Su version estructural interna no
  es una version del lenguaje.
- Los valores ordinarios se copian, pasan, asignan y retornan sin `move` ni
  `copy` obligatorios.
- `move`, `copy` y excepciones permanecen disponibles cuando el programador las
  solicita expresamente, pero su ausencia no impone diagnosticos.
- `unsafe` deja de conceder capacidades. Ninguna operacion nativa requiere un
  bloque, metodo, tipo o opcion de proyecto `unsafe`.
- `static_cast<T>` expresa conversiones conocidas por el sistema de tipos.
- `dynamic_cast<T>` expresa conversiones polimorficas comprobadas en ejecucion.
- `reinterpret_cast<T>` expresa reinterpretaciones deliberadas de punteros,
  enteros o representaciones compatibles con el backend.

## Semantica de ausencia

Un programa que no utilice estas formas conserva el comportamiento historico:
conversiones ordinarias, valores copiables, punteros tipados, FFI, uniones y
llamadas sin excepciones funcionan sin anotaciones adicionales.

## Frontera de trabajo

Se pueden modificar el parser, AST, semantica, tipos, metadata, proyectos,
driver, language service, backend de casts y las pruebas que codificaban
directamente la politica retirada. Permanecen fuera de esta tanda runtime,
bibliotecas, bindings, Saru, instalaciones y cualquier ruta en `C:`.

## Validacion

Cada commit funcional se verifica reconstruyendo en `A:` y ejecutando la
bateria CTest completa en O0 con NSLink. No se ejecutan baseline, minibaseline,
O1, O2, O3 ni deploy.

## Estado inicial

- Revision de inicio: `50f6cd22f`.
- Arbol ajeno preservado: `_tmp_strip_saru_exceptions.py` sin seguimiento.
- Evidencia externa: 106 fallos, de los cuales 94 comparten la activacion
  global de reglas por edicion y los restantes manifiestan movimiento,
  ownership o uniones forzados.
- Estado: en auditoria estatica.

## Cambio 10A: superficie acumulativa y politicas locales

### Causa

`Project::recognizedLanguageEdition()` seleccionaba el parser actual, mientras
`Project::enforcesLanguage3Effects()` y varias ramas de
`ProjectMethodBodyModel` convertian la etiqueta del proyecto en politicas
globales. Ademas, `Parser::parseStructDeclaration()` marcaba todo struct de la
edicion 3 como `ns3-value` y toda union como `active-member-safe`. La fuente no
solicitaba esas propiedades: eran inyectadas por el manifiesto.

### Correccion

- El parser reconoce la superficie acumulativa sin `NS3002`.
- `import`, `export import`, visibilidades, `match` y constraints dejan de estar
  ocultos por una etiqueta.
- Un struct ordinario ya no recibe `ns3-value`; vuelve a ser copiable por
  defecto.
- Una union ordinaria ya no recibe `active-member-safe`; leer un miembro no
  exige `unsafe`.
- Una etiqueta de proyecto ya no activa efectos obligatorios.
- Las validaciones de features explicitamente escritos (`throws`, referencias,
  propagacion Result y lambdas autonomas) se ejecutan por presencia del nodo,
  no por edicion.
- El analisis de escape no rechaza punteros crudos ordinarios; conserva la
  comprobacion para referencias, valores prestados, pinning y vistas que si
  declaran un contrato de lifetime.

### Archivos

- `src/Compilation/Project.cpp`: reconocimiento acumulativo y politica global
  de efectos desactivada.
- `src/Parser/Parser.cpp`: gates de edicion retirados y marcadores implicitos
  eliminados.
- `src/Frontend/ProjectMethodBodyModel.cpp`: reglas ligadas a nodos y contratos
  explicitos en lugar de una edicion global.

### Invariantes

- No se modifican runtime, bibliotecas, bindings, Saru ni instalaciones.
- No se introduce recursión.
- `move`, `copy`, referencias, excepciones y Result conservan sus nodos y
  lowering cuando aparecen de forma explicita.
- El cambio no implementa aun la retirada fisica del campo `<Language>` ni la
  eliminacion de su registro en NSMETA; esos son cambios separados.

### Verificacion

Pendiente de reconstruccion y bateria CTest completa O0 con NSLink.
