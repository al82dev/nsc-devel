# C-compatible unions / Uniones compatibles con C

## Español

`union` representa almacenamiento crudo superpuesto con layout C/Win64. No contiene tag y no
recuerda en runtime qué campo fue escrito. Para estados cerrados que deban ser siempre válidos se
debe usar `variant`, que sí conserva un discriminante y permite `match` exhaustivo.

```nativesharp
public union Word
{
    public int Signed;
    public uint Unsigned;
}

Word value;
value.Signed = 42;
int answer = value.Signed; // seguro: la escritura demuestra el miembro activo
```

La evidencia pertenece al almacenamiento, no al nombre del tipo. Una copia directa conserva el
miembro demostrado y un inicializador agregado activa el primer campo. Al unir control flow, la
evidencia sobrevive sólo si todas las ramas alcanzables escribieron el mismo campo.

Leer una alternativa distinta es reinterpretar bytes. Esa operación debe aparecer dentro de un
bloque `unsafe`:

```nativesharp
public union WordBytes
{
    public uint Whole;
    public byte First;
}

WordBytes value;
value.Whole = 42;
unsafe
{
    byte low = value.First;
}
```

El ejemplo depende deliberadamente del orden little-endian de Windows x64. `unsafe` no añade un
check ni un tag; documenta que el programador acepta el contrato de representación. Una lectura
inactiva fuera de ese bloque es error de compilación.

### Contrato FFI

- El tamaño es el máximo de sus campos, ajustado a la alineación máxima y a los modificadores de
  layout explícitos.
- Todos los campos comienzan en el mismo offset.
- La cabecera de host emite un `union` C junto con aserciones de tamaño y alineación.
- El contrato viaja en NSMETA: importar una biblioteca no convierte una unión cruda en un tipo
  seguro ni pierde la comprobación del miembro activo.
- Las funciones FFI que reciben una unión por valor no conocen su miembro activo. Deben accederlo
  dentro de `unsafe`, salvo que su propio control flow haya demostrado el campo.

## English

`union` is raw overlapping storage with the C/Win64 layout. It carries no tag and records no
runtime active-member state. Use `variant` for closed states that must remain valid: variants own a
discriminant and support exhaustive `match`.

A direct field write proves that field active. Aggregate initialization proves the first field,
and a direct union copy transfers the source proof. At a control-flow join the proof survives only
when every reachable path agrees on the same field.

Reading a different field reinterprets bytes and therefore requires an explicit `unsafe { ... }`
block. `unsafe` adds neither a tag nor a runtime check; it states that the programmer accepts the
representation contract, including assumptions such as Windows x64 little-endian byte order.

For FFI, size is the largest member rounded to the union alignment and every member starts at
offset zero. Generated host headers publish a real C union plus size/alignment assertions. NSMETA
preserves the raw-union contract across static-library and DLL boundaries, so imported code keeps
the same active-member checks.
