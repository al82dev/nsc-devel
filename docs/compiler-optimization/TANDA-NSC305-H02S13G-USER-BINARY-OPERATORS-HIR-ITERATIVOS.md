# NSC 3.0.5 — H02S13G: operadores binarios de usuario HIR iterativos

## Clasificación

`IMPLEMENTADO Y VALIDADO FOCALMENTE`.

## Alcance cerrado

- Migra únicamente la bajada de `BinaryExpression` resuelta contra un operador de usuario.
- Mantiene `lowerBoundCall` como autoridad común de resolución, conversiones, préstamos,
  argumentos opcionales, cleanup y resultados por referencia.
- No cambia parser, binding, sintaxis, diagnósticos, ABI, NSMETA, MIR ni LLVM.
- No invade las familias H02S13H en adelante.

## Causa

La ruta histórica reconocía el operador de usuario dentro de `lowerExpressionImpl` y volvía a entrar
por `lowerResolvedCall` con ambos operandos. Una cadena binaria asociada a la izquierda repetía esa
entrada una vez por nivel y agotaba la pila nativa antes de terminar el HIR.

## Diseño

La máquina de expresiones incorpora el frame `UserBinaryOperator`. El frame conserva el binding y
el target ya resueltos, baja primero el operando izquierdo y después el derecho, y aplica a cada uno
su preparación inmediatamente después de obtenerlo:

1. el `passKind` del parámetro decide si el descenso requiere dirección;
2. la conversión seleccionada se envuelve antes de continuar;
3. `wrapCallArgumentBorrow` conserva el orden de temporales y actualiza `hasOwnedTemporary`;
4. sólo entonces se baja el siguiente operando;
5. `finishBoundCall` reconstruye el mismo call, defaults, cleanup y posible `PointerLoad`.

La preparación inmediata es parte del contrato: preparar ambos operandos al final podría cambiar el
orden de los locales sintéticos creados por préstamos.

## Invariantes auditadas

- Orden observable estricto: operando izquierdo, preparación izquierda, operando derecho,
  preparación derecha.
- Cada operando se baja exactamente una vez.
- `Value`, `In`, `Ref` y `Out` conservan el mismo contexto de dirección que la ruta común.
- Conversiones seleccionadas, `memberOrdinal`, `staticCall`, `baseReceiver`, dispatch de interfaz,
  argumentos por defecto, cleanup y resultado por referencia permanecen bajo la autoridad común.
- La llamada recursiva histórica del binario de usuario se elimina.
- Las rutas builtin/core y las demás expresiones no cambian de clasificación.
- Todos los helpers nuevos de producción y prueba permanecen debajo de 60 líneas ejecutables; cada
  transición añadida a la máquina existente conserva una sola responsabilidad.

## Prueba puntual propia

El selector existente `NSC_PROJECT_OOP_LOWERING_FOCUS` incorpora exclusivamente:

`hir-user-binary-operator-depth`

El fixture compila un operador canónico `+(Node, Node)` por valor y construye una cadena asociada a la
izquierda de 32,768 niveles. El oracle recorre el HIR sin recursión y exige por nivel `StaticCall`,
dos hijos en orden, tipo, sintaxis y `memberOrdinal` exactos. En el mismo selector, un caso somero
sintético cambia los dos parámetros a `In`, aporta conversiones y borrows explícitos, y exige el
orden `OwnedTemporaryBorrow(BorrowValue(Conversion(ThisValue)))` con ordinales sintéticos 0 y 1.
Así se prueba que cada conversión/préstamo se prepara antes de descender el operando siguiente.

No se añade fuente ni target: el archivo ya pertenece a `nsc_project_oop_lowering_tests`, por lo que
no aplican cambios de CMake, CTest, manifiesto, catálogo ni NSC Builder 3.

## Validación autorizada

Se compilará únicamente `nsc_project_oop_lowering_tests` y se ejecutará únicamente el selector
`hir-user-binary-operator-depth`. No se ejecutarán la ruta default, CTest, familias TL ni baterías.

## Evidencia

- Configuración aislada: PASS, `A:\NSC-devel-paralelo\logs\agente-1\h02s13g-configure.log`.
- Target `nsc_project_oop_lowering_tests`: PASS, exit 0,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s13g-build-final2.log`.
- Selector `hir-user-binary-operator-depth`: PASS, 4 checks, exit 0,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s13g-focus-final2.log`.
- No se ejecutaron la ruta default, CTest, TL, minisuite, suite completa ni baseline.
