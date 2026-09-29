# Tanda 1: contrato normativo de funciones de namespace

## Objetivo

Definir la semántica que debe conservar la implementación multiparchivo de funciones libres antes de modificar el frontend. Este documento es normativo para la rama `enhancement` y para las tandas siguientes.

## Alcance

- Una función libre dentro de un proyecto `.nsproj` debe pertenecer a un namespace nombrado.
- El modo procedural histórico de un único archivo conserva sus funciones globales absolutas.
- El archivo fuente organiza declaraciones, pero no participa en su identidad.
- Varios archivos pueden contribuir al mismo namespace y el resultado no depende de su orden.
- Una función de namespace no pertenece a un tipo, no recibe `this`, no participa en virtual dispatch y no obtiene acceso privilegiado a miembros privados.
- El lowering puede compartir infraestructura con métodos estáticos, pero AST, símbolos, metadata, mangling, diagnósticos y depuración deben conservar la clase real del callable.

## Accesibilidad

- Sin modificador: `internal`.
- `internal`: visible sólo dentro del proyecto/módulo propietario.
- `public`: publicable mediante NSMETA y visible desde consumidores con `LibraryReference`.
- `private`, `protected`, `static`, `virtual`, `abstract`, `override`, `sealed` y `partial` se rechazan sobre funciones de namespace.

## Identidad y sobrecargas

La identidad semántica incluye:

1. módulo propietario;
2. namespace completamente calificado;
3. nombre;
4. aridad genérica;
5. tipos, ownership y modo `ref`/`out`/`in` de los parámetros;
6. convención de llamada;
7. marcador variádico cuando aplique.

El tipo de retorno no distingue sobrecargas, pero debe coincidir entre una declaración anticipada y su definición. Dos definiciones de la misma identidad son un error determinista con ambas ubicaciones.

## Lookup

Dentro de un cuerpo la búsqueda ocurre en este orden:

1. parámetros y locales;
2. parámetros genéricos del callable;
3. símbolos del namespace actual;
4. imports `using` del archivo;
5. símbolos públicos de referencias visibles.

Un acceso calificado distingue namespace, tipo y valor. Los candidatos ambiguos nunca se resuelven por orden de archivos o referencias. Un namespace y un tipo no pueden ocupar el mismo nombre completamente calificado dentro del mismo módulo.

## Declaraciones y definiciones

Una declaración sin cuerpo puede combinarse con una definición compatible en otro archivo del mismo módulo. Una declaración NativeSharp ordinaria que no obtenga definición es un error; los imports externos requieren una forma ABI explícita.

## Punto de entrada

Un ejecutable puede seleccionar una función mediante `<EntryPoint>Namespace.Main</EntryPoint>`. Sin esa propiedad se acepta exactamente un candidato elegible del proyecto raíz. Durante esta entrega la firma elegible continúa siendo `int Main()` sin parámetros. Un método estático existente sigue siendo válido, pero la coexistencia de varios candidatos exige selección explícita.

## Estado de namespace

La entrega incluye constantes, almacenamiento con inicialización cero e inicializadores constantes. La inicialización dinámica queda fuera de alcance y debe rechazarse explícitamente.

## Compatibilidad y plataforma

- Único target: Windows x64.
- Las funciones públicas se incorporarán en una extensión aditiva de NSMETA; el siguiente minor disponible se elegirá en la tanda de metadata.
- Native ABI 1.0 y Runtime ABI 1.0 no cambian por introducir un callable directo sin receptor.
- El modo procedural de un archivo y los métodos existentes conservan sus identidades físicas.
- No se crean clases sintéticas visibles ni internas como sustituto del modelo semántico.

## Verificación de esta tanda

- Revisión del parser procedural, `ProjectNameLookup`, `ProjectMemberModel`, HIR, MIR, driver y NSMETA existentes.
- Construcción incremental únicamente del objetivo `nsc`.
- No se ejecuta baseline, CTest general, fixtures generales ni matriz de optimizaciones.

## Archivos

### Nuevos

- `docs/2.0/development/namespace-functions/TANDA-01-CONTRATO-NORMATIVO.md`: autoridad de diseño para impedir que las tandas posteriores introduzcan clases falsas, identidades dependientes del archivo o cambios accidentales de ABI.

### Modificados

- Ninguno.

## Mejora obtenida

La característica ya tiene fronteras comprobables y decisiones de compatibilidad explícitas. Esto evita resolver contradicciones durante el lowering, cuando corregir una identidad equivocada resultaría mucho más costoso.
