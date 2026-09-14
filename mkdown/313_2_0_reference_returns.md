# 313 — Retornos `ref` / Reference returns

## Resumen / Summary

Esta entrega completa el segundo tramo de referencias nativas: un callable puede devolver una
ubicación mutable y el llamante conserva su identidad.

```nativesharp
private static ref int Select(ref int value)
{
    return ref value;
}

Select(ref counter) = 42;
ref int alias = ref Select(ref counter);
```

This delivery makes reference returns true address-preserving calls. Reading the result loads the
pointed value; assigning it or binding a reference local keeps the returned address.

## Contrato de lenguaje / Language contract

- La firma se escribe `ref T Nombre(...)` y el retorno `return ref ubicación;`.
- La ubicación debe ser mutable, direccionable y del tipo exacto `T`; no hay temporal ni conversión.
- Se admiten fields, parámetros `ref`/`out`, locales `ref` y resultados de llamadas `ref`.
- Se rechazan locales ordinarios del frame, parámetros por valor o `in`, literales y properties.
- Un callable ordinario no acepta `return ref`, y uno `ref` no acepta un retorno de valor.
- La llamada es un lvalue: lectura, escritura, asignación compuesta y reenvío observan el mismo slot.
- Overrides e implementaciones de interfaz preservan el tipo exacto y el modo de retorno `ref`.
- El alias no adquiere ownership ni extiende lifetime. Lambdas y `extern C` permanecen fuera de esta
  superficie; los puntos 315–316 consolidan procedencia, escapes y duración.

Reference returns are invariant mutable aliases. The compiler rejects hidden temporaries and
stack-owned value locations, and dispatch contracts include the ref-return mode. Ownership and
lifetime remain properties of the original storage.

## Diseño del compilador / Compiler design

El AST conserva una marca de referencia en declaraciones y `return`. El modelo de miembros la
convierte en `ClassMemberSymbol::isReferenceReturn`; el análisis de cuerpos usa las propiedades
existentes `addressable` y `assignable` para validar y propagar lvalues. Los modelos de herencia e
interfaces incluyen el modo en la coincidencia de firmas.

HIR y MIR separan dos hechos: `referenceReturn` describe la ABI del callable y
`referenceResult` describe una llamada. En contexto de dirección se propaga `ptr`; en contexto de
valor se agrega `PointerLoad`. LLVM declara y define el retorno físicamente como `ptr`, omite `sret`
y aplica la misma convención a llamadas estáticas, de instancia, virtuales e interfaz.

NSMETA 1.15 reserva el bit 24 para el retorno `ref`. La API sintética de un `LibraryReference`
reconstruye `ref T`, conserva el nombre físico del productor y emite la misma ABI. Una escritura
deliberada como 1.14 elimina el bit para no atribuir una capacidad que ese formato no conoce.

## Cobertura / Coverage

`tests/unit/Item313ReferenceReturnsTests.cpp` valida en O0:

- reenvío de parámetro, fields estáticos y de objeto, local `ref` y lecturas/escrituras;
- dispatch directo, virtual e interfaz con mutación del storage seleccionado;
- dumps semántico, HIR y MIR, LLVM verificado, enlace y ejecución real;
- rechazo de local del frame, literal, tipo distinto, retorno sin `ref`, uso en método ordinario,
  parámetro por valor y override cuyo modo no coincide;
- round-trip NSMETA 1.15, stripping 1.14 y consumo real de una `.lib` precompilada.

La prueba enfocada y las regresiones de lenguaje, atomics, metadatos y migración pasan. No se
ejecuta baseline completo entre puntos; la tanda 6 recibe una sola certificación al cierre.

## Archivos principales / Main files

- `include/nativesharp/AST/AstNode.h` y `src/AST/`
- `src/Parser/Parser.cpp`
- modelos de miembros, cuerpos, interfaces, dispatch, HIR y MIR en `src/Frontend/`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp` y
  `src/Driver/Driver.cpp`
- `tests/unit/Item313ReferenceReturnsTests.cpp` y el golden histórico NSMETA 1.14.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Implementa retornos ref / Implement reference returns`

**Descripción / Description**

`Conserva aliases mutables a través de firmas, llamadas, HIR/MIR, ABI LLVM y NSMETA 1.15;
certifica lectura, escritura, dispatch, reenvío y diagnósticos en O0. / Preserve mutable aliases
across signatures, calls, HIR/MIR, the LLVM ABI, and NSMETA 1.15; certify reads, writes, dispatch,
forwarding, and diagnostics at O0.`
