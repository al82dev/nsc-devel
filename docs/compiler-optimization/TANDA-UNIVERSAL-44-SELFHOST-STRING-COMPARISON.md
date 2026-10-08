# Tanda universal 44: comparación de cadenas por contrato declarado

## Causa confirmada

La tanda 43 permitió que `nsc2` terminara su auto-prueba, pero la emisión de `nsc3` cayó en
`SemanticType.Display()` con `this=0x40`. La pila y la memoria bajo `nsdbg` demostraron la cadena
completa:

- el nodo activo era el constructor `ArenaBlock`;
- `DeclareFunction` obtuvo `returnType = -1` al ejecutar `Types().Find("void")`;
- la tabla contenía 33 tipos y el tipo `void` estaba correctamente en el índice 1;
- el objeto emitido para `Find` comparaba los punteros de las cadenas con `cmpq`, no su contenido;
- `Get(-1)` leyó la cabecera del arreglo, cuya capacidad era 64 (`0x40`), como si fuera un objeto.

El hecho semántico de una llamada a método podía quedar provisionalmente como `error` aunque el
símbolo seleccionado declarara retorno `string`. El backend clasificaba cadenas únicamente por el
hecho provisional y degradaba `Display() == display` a identidad de puntero.

## Cambio

La clasificación de expresiones string ahora usa dos fuentes, en orden:

1. el tipo semántico resuelto de la expresión;
2. para llamadas o miembros cuya inferencia quedó incompleta, el tipo de retorno declarado por el
   símbolo seleccionado.

Las dos rutas de comparación del backend comparten esa clasificación. `==` y `!=` de cadenas se
emiten mediante el comparador nativo de longitud y bytes; los demás punteros conservan identidad de
puntero. No se alteraron pruebas ni expectativas.

## Archivos

- Modificado: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Nuevo: este documento.

## Validación dirigida

1. Regenerar solamente SH41 en O0.
2. Verificar en el objeto de segunda generación que `TypeTable.Find` ya invoque la comparación de
   contenido.
3. Confirmar que la emisión nsc2 a nsc3 deje de leer `types[-1]`.
4. Mantener abiertos SH41 y sus cierres derivados hasta la corrida integral O0 con NSLink.
