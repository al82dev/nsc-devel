# T09: tamaños ocultos en métodos estáticos C++

Los métodos estáticos C++ con `pass_object_size` o
`pass_dynamic_object_size` se descartaban al indexar la clase. NSC conserva
ahora el modo por parámetro, inserta el tamaño oculto en la declaración y en
la llamada, y conserva el plegado de índices locales constantes. Rechaza la
conversión de estos métodos a punteros de función, porque la firma física
incluye un argumento adicional.

## Validación

- Construcción incremental de `nsc`; se comprobó el contrato del runtime del
  host. La construcción recompiló dependencias de los símbolos de miembros.
- `pruebas-tanda-09-cpp-tamano-objeto/validar-metodo-estatico.py
  --precompilado --no-generico`: ocho modos, índice constante de subobjeto y
  booleano posterior al tamaño oculto; comparación C++/NativeSharp en O0–O3
  para biblioteca estática y DLL. Se inspecciona la declaración y llamada LLVM
  del booleano, se rechaza convertir el método con tamaño a puntero y se
  comprueba que un método ordinario sí pueda convertirse y ejecutarse.
- Dieciséis consumidores de la biblioteca NativeSharp no genérica, con y sin
  encabezado, pasaron con las mismas configuraciones. Evidencia:
  `A:/temp/nsc-t09-size-static-method-t6e838th`.
- `nsc.cpp-interop.session` no pudo ejecutarse: falta
  `cpp-fixtures/project/CppProbe.nsproj` en la raíz configurada de la laptop.
  No se alteró la prueba. La cobertura anterior es de compilación y ejecución
  con Clang real, no sustituye esa prueba de sesión.

## Pendiente y límites

Una plantilla NativeSharp precompilada que llama directamente al método C++
falla al reconstruirse: el consumidor no encuentra el tipo/método de la
dependencia, aun con el encabezado. Reproducción:
`A:/temp/nsc-t09-size-static-method-7fvf647c`. El catálogo de records no
transporta todavía este contrato de métodos; queda por resolver en el contrato
vigente, sin versionamiento histórico. La prueba explícita
`--precompilado` conserva ese caso abierto; `--no-generico` prueba la ruta
ya implementada. Métodos de instancia, constructores, otras convenciones y
combinaciones ABI tampoco quedan acreditados. T09-003 sigue abierto.
