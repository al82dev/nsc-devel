# Tanda 08: impedir reasignación de arreglo global C

- La primera validación agrupada compiló `shared_array = shared_array`, por lo que falló el contrato de arreglo C no reasignable.
- Se ajusta únicamente la asignabilidad de estados externos de arreglo fijo. El estado sigue siendo direccionable y el indexador conserva la asignabilidad de elementos mutables.
- Pendiente de repetir la prueba negativa, el acceso indexado y la batería agrupada; la corrección no cambia la semántica de arreglos NativeSharp.
