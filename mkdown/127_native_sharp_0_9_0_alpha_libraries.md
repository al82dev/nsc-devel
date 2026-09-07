# NativeSharp 0.9.0 — bibliotecas primero, cierre estable

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

## Primer cierre: NSMETA 1.3, 1.4 y 1.5

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

`NSMETA 1.5` conserva la firma completa de un tipo delegate: representación `extern C`, retorno y
tipos de parámetros. Esto permite que un consumidor reconstruya callbacks precompilados en vez de
conocer únicamente el nombre nominal del delegate.

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
- especializaciones genéricas cerradas conservadas por metadata;
- tablas virtuales e interfaces preservadas sin degradar su semántica;
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

## Cuarto cierre: structs, enums y receptores de instancia

La siguiente rebanada deja de limitar la superficie precompilada a tipos base. Un método público
puede usar ahora un `struct` o `enum` definido por el productor en parámetros y retorno; el
consumidor obtiene su declaración desde `.nsmeta`, sin copiar fuentes ni escribir un espejo manual.

La reconstrucción no crea un `struct` vacío con el mismo nombre. Para cada tipo de valor emite una
declaración sintética con cuatro contratos físicos tomados del productor:

- `pack(alignment)` conserva una alineación reducida por empaquetado;
- `align(alignment)` conserva una alineación aumentada;
- `at(offset)` fija cada campo en la posición publicada;
- `size(size)` obliga al modelo de layout del consumidor a volver a demostrar el tamaño completo.

Esto cubre layout natural, relleno, structs anidados, `pack`, unions representables por offsets
explícitos y alineación aumentada sin inventar una segunda calculadora ABI. La misma fase ordinaria
que valida un struct escrito por el usuario valida la declaración reconstruida. Si NSMETA afirma
que un `Packet` mide 12 bytes pero sus campos publicados reconstruyen 13, la compilación se detiene
antes de producir IR o invocar al linker e informa ambos tamaños.

Los enums se reconstruyen con su tipo entero subyacente exacto y sus constantes públicas. El
consumidor distingue así `byte`, `sbyte`, `int`, `long` y sus variantes sin deducir signo a partir
del tamaño. Sus valores viajan como el patrón de bits que ya conserva NSMETA.

La entrega incluye además métodos de instancia públicos sobre structs. El receptor implícito
atraviesa el mismo mangling y ABI que produjo la biblioteca: `Pair.Offset()` modifica el valor y
`Pair.Sum()` lo lee desde código que vive dentro de la `.lib` o DLL. No se reemite el cuerpo en el
consumidor. Para DLL, esos métodos mangled se incorporan ahora al export set; antes el filtro del
driver publicaba exclusivamente métodos estáticos aunque la metadata describiera el receptor.

No se habilitaron todavía métodos de instancia sobre clases. Importarlos sin cerrar constructor,
ownership, destructor y RTTI permitiría fabricar una referencia sin contrato de vida. La frontera
actual es deliberada: structs por valor y sus receptores; clases continúan como contenedores de
métodos estáticos hasta la siguiente rebanada.

### Composición transitiva nominal

La prueba A → B ya no enlaza únicamente una función C. B compila usando `Pair`, `Echo`, `Offset` y
`Sum` publicados por A; después el consumidor final referencia sólo B. La variante estática
consolida los objetos de A dentro de B. Una segunda variante construye una DLL B con A como
dependencia estática: el PE final importa únicamente B, no inventa una dependencia dinámica hacia
A, y ejecuta el resultado correcto.

### Evidencia de la rebanada

La suite central de bibliotecas creció de 74 a **93 comprobaciones** y cubre:

- struct natural `Pair` por parámetro y retorno;
- struct `Packet` empacado de 13 bytes con un struct `Packed` anidado;
- enum `Mode : byte` y sus constantes;
- métodos de instancia mutador y lector;
- `.lib` y DLL, O0 y O2, con ejecución que devuelve 42;
- metadata con fingerprint recalculado pero tamaño nominal falso, rechazada antes del enlace;
- composición estática A → B y DLL B → ejecutable con A estática;
- paridad de las 93 comprobaciones con lld-link y NSLink.

Esta rebanada no aumenta el formato: NSMETA 1.4 ya contenía identidad, campos, offsets, tamaños,
alineación, tipo subyacente y miembros. El defecto estaba en el consumidor, que descartaba los
métodos con tipos nominales y sólo reconstruía sus contenedores estáticos.

### Revalidación acumulada

El catálogo nativo completo ya había cerrado **296/296** con lld-link. Con NSLink, la corrida
paralela terminó inicialmente 294/296 por dos bloqueos transitorios de Windows (`user-mapped
section open`); ambos casos pasaron al repetirse en serie, por lo que el resultado funcional es
también **296/296**. El host C# P/Invoke pasó **1/1** fuera del aislamiento con cada linker. Total
efectivo: **297/297** con lld-link y **297/297** con NSLink.

Después del cierre nominal y del refuerzo de validación física, la matriz directamente afectada
se reconstruyó y volvió a pasar 4/4 con cada linker. Esta matriz incluye driver, Stage 46,
metadata de bibliotecas y backend de enlace de Windows. El gate G00 oficial debe ejecutarse de
nuevo porque esta entrega mueve el árbol.

## Quinto cierre: clases opacas y ownership entre módulos

Una clase pública del productor puede cruzar ahora la biblioteca como referencia opaca. El
consumidor no reconstruye ni toca sus campos: una factory del productor asigna el objeto, los
métodos concretos no virtuales reciben el mismo receptor nativo y un sink `owned` consume la
referencia mediante `move`. La destrucción permanece en el módulo que conoce el layout.

El carrier sólo se acepta cuando tamaño y alineación coinciden con el puntero del target. Una
metadata autofirmada que anuncia 4/8 para una clase en un contrato x64 de 8/8 se rechaza antes del
enlace. Los métodos virtuales, overrides e interfaces no se convierten en llamadas directas: se
conservan en metadata y quedan fuera de la materialización hasta que exista hidratación real de
dispatch precompilado.

La prueba descubrió además una regla importante de ownership: un parámetro `owned` ya ejecuta su
cleanup al salir. Hacer `delete` manual dentro del sink duplica la destrucción. El fixture final
usa un sink vacío, transfiere con `move` y demuestra una sola liberación en `.lib` y DLL, O0/O2.

## Sexto cierre: NSMETA 1.5 y callbacks `extern C delegate`

NSMETA 1.5 añade al registro de tipo delegate su representación `extern C`, retorno canónico y
lista ordenada de parámetros. Esos datos participan en la huella ABI y permiten reconstruir en el
consumidor un callback como `int Transform(int)` sin copiar la fuente del productor.

Los lectores actuales siguen aceptando 1.0–1.4. Una prueba específica escribe y vuelve a leer un
modelo 1.4 para demostrar que la ausencia de los campos 1.5 no desplaza los campos ni miembros que
siguen en el payload. Sólo los delegates `extern C` se materializan en 0.9; un delegate con
closure/target conserva su identidad pero queda para la frontera de 1.0.

## Promoción y evidencia final de 0.9.0

Con los seis cierres completos, la identidad compatible dejó `0.9.0-alpha` y pasó a `0.9.0`. La
suite Stage 46 creció de 74 a **111 comprobaciones** con cada linker; Stage 48.2 terminó 72/72 y
Stage 50.9 terminó 47/47. El catálogo acumulado da **297/297 efectivos** con lld-link y
**297/297 efectivos** con NSLink.

La corrida final del producto estable obtuvo 288 verdes directos; ocho emisiones de objeto que
Windows rechazó temporalmente por una sección mapeada pasaron 8/8 al repetirse aisladas, y el host
C# P/Invoke pasó 1/1 con acceso al SDK. El binario exacto es
`nsc 0.9.0+build.20260907T042535Z.03ca780e6debcd15`, SHA-256
`ADB3BAF106CDC666D7E005436280FE2DECC7362530FB2E13D0057EF794622FC1`. Sólo falta instalar ese
artefacto y ejecutar G00 sobre el mismo hash para convertir el cierre local en baseline
certificado.

La reconstrucción limpia detectó una expectativa histórica de Stage 47 que todavía buscaba el
diagnóstico “public static method”. Desde que 0.9 publica también métodos concretos de instancia,
el contrato correcto es “publishable public method”. Se actualizó únicamente la fixture, sin
relajar el rechazo de una DLL sin superficie pública; Stage 47 terminó **80/80**.
