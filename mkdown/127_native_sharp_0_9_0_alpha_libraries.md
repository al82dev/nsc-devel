# NativeSharp 0.9.0-alpha — bibliotecas primero

Cada pasada de esta línea obedece la regla de generación descrita en
[`128_regla_de_generacion_por_pasada.md`](128_regla_de_generacion_por_pasada.md): una pasada no está
cerrada hasta producir, identificar y registrar el hash de su `nsc.exe` acumulado.

## Decisión de alcance

La serie `0.9` comienza en la frontera binaria del compilador. El objetivo es que un productor
NativeSharp pueda publicar una DLL o una biblioteca estática y que otro proyecto pueda consumirla
sin depender del árbol fuente del productor ni perder información del contrato.

Este trabajo cubre exclusivamente:

- DLL de Windows;
- import libraries COFF asociadas a la DLL;
- bibliotecas estáticas COFF;
- metadata `.nsmeta`;
- ABI, mangling, exports e imports;
- tipos, genéricos, ownership y dispatch que cruzan la biblioteca;
- enlace, diagnóstico y compatibilidad entre versiones.

## Primer cierre: NSMETA 1.3 y 1.4

`NSMETA 1.2` ya conservaba el nombre, tipo y ownership de cada parámetro, pero omitía su valor
predeterminado. Por ello una firma podía compilar correctamente mediante `ProjectReference` y aun
así perder parte de su contrato al convertirse en biblioteca precompilada.

`NSMETA 1.3` añade a cada parámetro:

- presencia y clase del literal predeterminado;
- signo separado para literales enteros;
- magnitud entera o de carácter;
- valor booleano;
- bits IEEE-754 del literal flotante ya parseado;
- contenido de cadena;
- representación explícita de `null`.

El lector actual continúa aceptando `NSMETA 1.0`, `1.1` y `1.2`. Los formatos anteriores no reciben
un default inventado: el parámetro se considera obligatorio porque ese dato no existía en el
archivo. La huella del contrato incluye los defaults, de modo que cambiar uno modifica la identidad
de la API publicada aunque la firma máquina permanezca igual.

`NSMETA 1.4` completa la identidad fuente del parámetro con `Value`, `ref`, `out` o `in`. Aunque
los tres últimos usan un puntero como carrier nativo, no son el mismo overload ni imponen las
mismas reglas al llamador. El lector conserva compatibilidad con `1.0–1.3`; un parámetro antiguo se
lee como `Value`, porque el formato anterior no contenía un dato del que pudiera inferirse otra
cosa con seguridad.

## Orden de trabajo de 0.9

1. Cerrar round-trip de contratos públicos en `.nsmeta`.
2. Consumir bibliotecas precompiladas desde el proyecto sin volver a parsear sus fuentes.
3. Resolver llamadas nativas contra DLL/import library y `.lib` estática con la misma semántica.
4. Congelar mangling, ABI, layout, ownership y dispatch públicos.
5. Validar dependencias transitivas y combinación de varias bibliotecas en un ejecutable.
6. Detectar metadata/binario incompatibles antes del enlace.
7. Hacer transaccional la salida del producto: binario, import library, header y metadata coherentes.
8. Certificar Debug/Release y ambos caminos de enlace soportados con ejecución real.

## Gate de salida

`0.9` no se considera cerrada sólo porque los archivos existan. Deben pasar productores y
consumidores separados para DLL y estática, sin fuentes compartidas, en estas familias:

- funciones y overloads;
- argumentos nombrados y predeterminados;
- structs, enums, clases opacas y delegates;
- ownership en parámetros, retornos, campos y cleanup;
- genéricos y constraints definidos en otra biblioteca;
- virtual dispatch e interfaces;
- imports externos y dependencias transitivas;
- O0/O2 y diagnósticos por símbolo, ABI o metadata incompatible.

Los tres primeros elementos ya tienen una rebanada vertical ejecutable: `NSMETA 1.3` conserva el
contrato, `LibraryReference` vincula sus tres productos y el frontend materializa la superficie C
pública sin exigir una redeclaración manual en el consumidor.

## Segundo cierre: consumo físico de `.lib` y DLL

El proyecto acepta ahora una referencia precompilada explícita:

```xml
<LibraryReference Include="../Math/NativeMath.lib"
                  Metadata="../Math/NativeMath.nsmeta"
                  Runtime="../Math/NativeMath.dll" />
```

`Metadata` puede omitirse cuando comparte nombre y directorio con el `.lib`. `Runtime` sólo se
declara para una DLL. La referencia se resuelve desde el proyecto que la contiene, se normaliza y
se incorpora automáticamente al enlace; ya no requiere repetirla como argumento del driver.

Antes de invocar al enlazador, `nsc` comprueba:

- estructura COFF real del `.lib`;
- versión, target, tamaño de puntero y huella ABI del `.nsmeta`;
- identidad de módulo única frente al grafo fuente y las demás bibliotecas;
- presencia en el índice COFF de cada símbolo C prometido por el metadata;
- firma exacta de retorno y parámetros entre el `extern C` consumidor y el contrato publicado;
- estructura PE y tabla exacta de exports del DLL declarado;
- identidad por nombre entre el DLL declarado y el DLL que el import library hace importar al PE
  final.

Los consumidores separados de la prueba ejecutan tanto la variante estática como la DLL y
devuelven `42`. Las pruebas negativas cubren firma ABI falsa, `.lib` emparejado con metadata ajeno
y un import library emparejado con otro DLL que publica símbolos de igual forma. También se hizo
interna la implementación RTTI auxiliar que antes producía símbolos fuertes duplicados al unir
dos módulos NativeSharp.

Una biblioteca estática que depende de otra `.lib` ya no deja esa dependencia escondida para el
enlace final: `nsc` consolida dentro del archive nuevo los objetos de las bibliotecas referenciadas
sin vendorizar las import libraries del proceso. La prueba A → B → ejecutable hace que el
consumidor declare y enlace únicamente A, comprueba que el índice de A contiene los símbolos de
ambos productos y ejecuta el resultado. Los imports mínimos que LLVM o SEH pueden introducir en
un objeto precompilado se materializan una sola vez durante el enlace final. Así una composición
no repite `KERNEL32`/UCRT dentro de cada `.lib` y produce el mismo contrato con NSLink y lld-link.

El linker PE y el librarian son herramientas distintas. `NSC_LLD_LINK` selecciona el primero;
`NSC_LLVM_LIB` selecciona el segundo. Si el linker es `lld-link`, `nsc` también puede usar su modo
`/lib`; un linker alternativo nunca recibe `/lib` por suposición. Esta separación corrigió el gate
NSLink que interpretaba archivos archive como objetos COFF (`NSL1003`, máquina `0x3C21`).

La inspección `nsc --print-linker` refleja esa separación: imprime `linker`, `librarian` y el
contrato `flavor: Windows COFF/PE`. Su fixture valida el contrato y no el nombre `lld-link`, porque
una pasada NSLink correcta debe seguir siendo verde cuando el archivador resuelto es `llvm-lib`.
La corrección fue revalidada dentro del catálogo completo: 296 casos nativos con NSLink más el
host C# ejecutado con acceso al SDK local, para un total efectivo de **297/297**.

## Tercer cierre: consumo semántico desde `.nsmeta`

El consumo precompilado ya no termina en el enlazador. Cada `LibraryReference` introduce en el
grafo un nodo de módulo precompilado cuya identidad canónica es la publicada en `.nsmeta`. Ese
nodo tiene propietario y ruta de metadata propios, participa en las mismas reglas de visibilidad
que un `ProjectReference` y permite detectar una identidad repetida antes del lowering.

Para esta superficie semántica, el driver reconstruye declaraciones NativeSharp a partir de los
métodos estáticos públicos cuyo contrato puede materializar por completo. Las declaraciones se agregan al
`SourceManager` y atraviesan el frontend ordinario: parser, agregación de tipos, resolución de
miembros, argumentos, HIR, MIR y LLVM. No existe una tabla lateral que salte la semántica del
lenguaje. Una unidad marcada como precompilada aporta contrato, pero no vuelve a emitir layout,
RTTI ni dispatch del productor; el código continúa viviendo exclusivamente en la `.lib` o DLL.

Por ello un consumidor puede escribir:

```csharp
public class Program
{
    public static int Main()
    {
        return Precompiled.Math.Add(left: 20);
    }
}
```

El nombre `left` y el default `right = 22` proceden de `NSMETA`; el overload se selecciona con las
reglas normales de NativeSharp. `NSMETA 1.4` reconstruye además `ref`, `out` e `in` sin confundirlos
con un puntero escrito por el usuario. El mismo fuente funciona contra una `.lib` estática y contra
el import library de una DLL, y ambos ejecutables devuelven `42`. No hay declaración `extern C` en
el consumidor.

El DLL publica ahora dos superficies complementarias: los nombres C explícitos y estables para
hosts nativos, y los símbolos mangled de sus métodos NativeSharp públicos para otro proyecto
NativeSharp. Por ello una DLL puramente NativeSharp puede existir sin agregar un export C ficticio.
La import library contiene ambas superficies y `.nsmeta` decide cuál identidad corresponde a cada
llamada.

La frontera de visibilidad también queda cerrada para esta superficie: un método privado no se
publica y no puede resolverse desde el consumidor. Si dos referencias intentan aportar la misma
identidad canónica de módulo/tipo, la compilación falla por ambigüedad antes de generar IR. Las
comprobaciones físicas y ABI del cierre anterior continúan aplicándose después de la resolución
semántica.

Este cierre importa métodos NativeSharp estáticos con tipos base, overloads, defaults, ownership y
modos de paso. No afirma todavía que los tipos nominales por valor/referencia, miembros de
instancia, genéricos ni tablas de dispatch puedan consumirse desde binario. Un método que exige uno
de esos contratos todavía no materializados se omite de la superficie sintética; no invalida toda
la biblioteca y tampoco recibe un ABI inventado. La expansión debe conservar esta misma
arquitectura: metadata como módulo canónico dentro del frontend, nunca como un segundo lenguaje de
resolución paralelo.
