# T13-019 y T13-020: COMDAT y la misma especialización desde dos módulos NativeSharp

## Pregunta

Un miembro de una especialización de plantilla, una función `inline` o una especialización de plantilla
de función tiene enlace vago: cada unidad de traducción que la usa la define. NSC emite esas definiciones
con Clang en cada módulo NativeSharp que las usa (cambios [1](t13-especializaciones.md) y
[2](t13-plantillas-ns.md)), y la biblioteca C++ también puede definirlas. La auditoría comprueba qué
definición queda en el ejecutable con cada enlazador que NSC puede usar.

## Auditoría

- Clang emite cada definición de enlace vago como `linkonce_odr` en un COMDAT `any`; `emitCppInlineIR`
  la pasa a `weak_odr` para que el optimizador de NSC no la elimine, y conserva el COMDAT. En COFF el
  COMDAT `any` es una sección con selección `IMAGE_COMDAT_SELECT_ANY`: el enlazador toma una de las
  definiciones y descarta las demás.
- NSLink no se modificó ni se inspeccionó por dentro: su selección se observa en el resultado. NSC elige
  el enlazador con `NSC_LINKER` (`nsc --print-linker` lo confirma), así que el mismo programa se enlaza
  con NSLink y con lld-link.
- Una especialización se nombra igual en toda sesión (hash de su tipo canónico), y sus miembros tienen el
  nombre decorado de Clang, así que dos módulos piden el mismo símbolo.

## Evidencia

Fixture nuevo `pruebas-tanda-13-comdat/validar.py`, con y sin `--dll`:

- la biblioteca C++ (Clang, O0) usa `Box<int>::get` y `Box<int>::twice` en `libGet`, así que su objeto
  las define; `llvm-readobj --symbols` comprueba que cada una está en una sección COMDAT de selección
  `Any`;
- un módulo NativeSharp precompilado (`Producer.lib`, `--emit-project-oop-lib`) escribe `T13.Box<int>`,
  llama a esos miembros y a `libGet`; su biblioteca define los mismos símbolos, también en COMDAT `Any`;
- el ejecutable consume `Producer.lib`, escribe `T13.Box<int>` y llama a los mismos miembros, que NSC
  vuelve a emitir.

El ejecutable enlaza con NSLink y con lld-link, de O0 a O3, y devuelve 36 como el control C++ de Clang
23.1.0: tres módulos definen los mismos miembros y queda una definición.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, NSLink y lld-link) | `A:/temp/nsc-t13-comdat-ztwbpawk` |
| DLL | pasa (O0–O3, NSLink y lld-link) | `A:/temp/nsc-t13-comdat-yig6thfa` |

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- La selección `Any` no comprueba que las definiciones sean iguales; que lo sean lo garantiza que todas
  vienen del mismo encabezado con la misma configuración (ODR), como en C++.
- Un módulo precompilado no exporta en su API pública un tipo de especialización C++; cada módulo la
  importa de su propia sesión.
