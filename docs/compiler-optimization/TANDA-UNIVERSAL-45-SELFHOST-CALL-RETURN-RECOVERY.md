# Tanda universal 45: recuperación del retorno declarado de llamadas

## Evidencia posterior a la tanda 44

SH41 siguió cayendo en la misma instrucción y el objeto conservó la comparación de punteros dentro
de `TypeTable.Find`. La causa restante era más precisa: el hecho del `CallExpression` y el hecho de
su `MemberExpression` no conservaban siempre el símbolo del método seleccionado. Consultar solamente
`Fact(...).SymbolId()` no alcanzaba la declaración `SemanticType.Display`, aunque el lowering normal
de llamadas sí la recuperaba por receptor, nombre y aridad.

## Cambio

`StringExpression` usa ahora el mismo orden de recuperación que la emisión de llamadas:

1. símbolo semántico directo, si existe;
2. tipo del receptor para fijar el padre preferido;
3. selección de la mejor sobrecarga por nombre y argumentos;
4. búsqueda por nombre, aridad y padre como cierre determinista.

Una vez localizada la declaración, su retorno fuente determina si la expresión es string. Esto no
convierte otros punteros en cadenas: sólo se aplica cuando una declaración resoluble declara retorno
`string`, `owned string` o `borrowed string`.

## Archivos

- Modificado: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Nuevo: este documento.

## Validación dirigida

1. Regenerar solamente SH41 en O0.
2. Confirmar que `TypeTable.Find` invoque comparación por contenido.
3. Verificar que el constructor `ArenaBlock` obtenga el tipo `void` y que nsc2 avance en la emisión
   de nsc3.
4. No cerrar los fallos self-host hasta la corrida integral O0 con NSLink.
