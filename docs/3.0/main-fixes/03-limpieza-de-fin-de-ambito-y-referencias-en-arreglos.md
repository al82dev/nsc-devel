# Limpieza de fin de ámbito y referencias guardadas en arreglos

Rama `development-main-fixes`. Campaña: mejora "referencias de clase guardadas en arreglos".
Evidencia: `A:\nsc-main-fixes-evidencia\claude\probes\SUMMARY-despues-ambito.txt` y
`A:\nsc-main-fixes-evidencia\claude\CAUSA-fin-de-ambito.txt`.

## SCOPE LOCK

- Objetivo: diagnosticar el uso de un elemento de arreglo cuyo objeto fue destruido por la limpieza
  automática de un local `raii` al terminar un bloque anidado, conservando los usos válidos.
- Naturaleza: correctiva (seguridad de vida) dentro del replay de referencias en arreglos.
- Revisión de partida: 93c276564. Archivos: `StatementAnalyzer.cpp`, `Internal/BlockFrame.h`,
  `MethodBodyCallableLifecycle.cpp`, `EscapeAnalyzer.cpp`, `ProjectMethodBodyModel.h`,
  `NativeSharp3T06EscapeSummaryTests.cpp`, este documento. Protegidos y prohibiciones: documento 01.

## Reproducción

`claude\probes\P19-fin-de-ambito` (solo análisis, no se ejecutó):

```nativesharp
MyClass[] references = new MyClass[1];
{
    MyClass inner = new MyClass();
    holder.StoreReference(inner, references);
}
int after = references[0].GetValue();
```

Antes (93c276564) se aceptaba sin diagnóstico.

## Causa

1. El replay no modelaba la destrucción de locales `raii` al salir de su bloque: solo
   `move` a un parámetro `owned` con efecto `Destroyed` marcaba el objeto como muerto.
2. Solo se reproducían llamadores con una transferencia `MoveLocalToOwnedParameter`.
3. La cobertura lineal rechazaba cualquier bloque anidado.

## Cambio

- `BlockFrame::ownsScope`: el frame recuerda si creó su ámbito.
- `replayArrayReferenceScopeExit` (llamada desde `finishBlockFrame` solo cuando el frame creó el ámbito
  y el flujo es alcanzable): cada local `raii` no estático del ámbito que aún posee una entrada en el
  replay se limpia con `cleanup()` y se registra un evento `Destruction` con el bloque y la
  declaración. Un local ya movido no tiene entrada; un dueño unido o ausente queda `MaybeDead`.
  `return`, `break` y `throw` no pasan por aquí y conservan el estado previo (no producen diagnósticos).
- Candidatos del replay: además de los existentes, un llamador que llama a un método con resumen de
  escritura en arreglo, tiene cobertura lineal y declara en un bloque anidado un local inicializado
  con `new` (`hasNestedScopedOwnerDeclaration`).
- Cobertura lineal: los bloques anidados simples (`auxiliaryText` vacío) se aplanan en orden; los
  bloques marcados (listas de declaración, `unsafe`, `defer`, `match`) siguen sin cobertura.
- `stopsBeforeRead`: un evento `Destruction` de fin de bloque ejecuta la cadena de destructores del tipo
  creado; si algún destructor no tiene salida normal la lectura posterior no se alcanza y no se
  diagnostica. Si el sitio de construcción no se encuentra, tampoco se diagnostica (falla cerrado
  hacia la aceptación previa).

## Pruebas

- `nsc.ns3.t06.escape-summaries` (`scopeExitControl`): rechazo como receptor y como copia después del
  bloque; aceptación de lectura dentro del bloque, de ranura que guarda otro dueño vivo y de destructor
  que no retorna. Pasan todas las expectativas previas del archivo.
- `nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins`,
  `nsc.stage56.c7-definite-assignment` pasan.
- Sondas: P19 rechazada; P05, P06, P07, P08b, P09b, P20 aceptadas; P01, P02, P04, P13, P21, P22
  conservan su diagnóstico. `repro-alias-original-clean\ClassDanglingReference.nsproj` sigue
  diagnosticando solo la lectura posterior al `delete` (línea 23).

## Límites

- Salidas de bloque por `break`/`continue`/`return` hacia código que lee el arreglo no marcan la
  destrucción (falsos negativos posibles, nunca falsos positivos por este cambio).
- `using`, `try`, bloques marcados y ciclos no reciben la limpieza modelada.
- Siguen sin diagnóstico P10, P11, P14, P16, P17 y P18 (ver documento 02).
