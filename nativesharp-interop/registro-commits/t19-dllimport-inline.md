# T19-008: funciones y miembros inline C++ declarados `dllimport`

Fixture nuevo: `pruebas-tanda-19-dllimport-inline/validar.py`, con y sin `--dll`. Sonda de desarrollo:
`tools/t19c/d10/sonda-t19-008.py` (`logs/sonda8.log`).

## Revisión

T10-012 dejó como límite que «las definiciones inline `dllimport` no se emiten: Clang les da enlace
`available_externally` y la DLL las exporta». La sonda comprobó, frente a Clang 23.1.0 a O0 y O2, que NSC ya
hace lo mismo que Clang con cada forma:

- `appendCppInlineObjects` no ofrece a Clang los símbolos `dllimport` (T10-012), así que NSC no emite una copia
  propia;
- la función importada lleva `cppDllImport`, así que NSC la declara `dllimport` y la llama por `__imp_`, como el
  control de Clang a O0. A O1–O3 Clang puede expandir su copia `available_externally`; el resultado es el mismo,
  porque la DLL tiene la definición que exporta.

No hubo cambio de código. Lo que faltaba era la evidencia, que ahora da el fixture.

## Qué funciona, como en Clang

Con `API` como `dllexport` al construir la DLL, `dllimport` al consumirla y vacío con la biblioteca estática:

- funciones `inline` y `constexpr` (`sq`, `cube`) junto a una fuera de línea (`base`);
- métodos inline de una clase exportada: de instancia (`S::get`), estático (`S::twice`), constructor y destructor
  (`W`) y un operador miembro (`P::operator+`, por su adaptador de T12-004);
- una función inline exportada que llama a otra que no lo es (`wrap` y `helper`): la DLL usa su copia de `helper`
  y el programa emite la suya, como Clang;
- un miembro estático `inline` de datos (`K::n`) y su lector inline: el programa escribe en la copia de la DLL;
- una función inline con una variable estática local (`next`): el programa y la DLL incrementan el mismo contador,
  porque solo la DLL tiene la definición. Con la biblioteca estática, la copia que emite Clang para el programa y
  la de la biblioteca se fusionan por COMDAT y el contador también es uno.

Todo pasa también desde una plantilla NativeSharp precompilada, con y sin el encabezado en el consumidor, salvo el
constructor, que sigue en T19-016 con los demás objetos C++ con constructores.

El fixture compara el resultado con un control C++ de Clang, O0–O3, y exige que cada declaración del IR de Clang
esté en el de NSC con los mismos tipos físicos y, en la DLL, también `dllimport`. Los operadores miembro se llaman
por su adaptador, así que su símbolo no se compara.

## Rechazo genuino

Una variable `inline` declarada `dllimport` con inicializador: Clang rechaza el encabezado (`definition of
dllimport data`) y NSC muestra ese mismo diagnóstico.

## Evidencia

Resultados: directo 712723 y desde la plantilla precompilada 861703, iguales a los de Clang en las ocho
combinaciones.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-dllimport-inline-f9v_kct_` |
| DLL | `A:/temp/nsc-t19-dllimport-inline-eoy5s_md` |

No se modificó Clang ni NSLink. El contrato NSMeta no cambia.
