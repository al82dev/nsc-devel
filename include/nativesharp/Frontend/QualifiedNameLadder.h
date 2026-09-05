#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
// LA ESCALA CON LA QUE UN NOMBRE ESCRITO SE CONVIERTE EN UN NOMBRE COMPLETO, y por que vive aqui
// y no dentro de quien la usa.
//
// `C.F<int>(0)` escrito dentro de `A.B` no nombra a cualquier `C`: nombra al primero que aparece
// bajando la escala --contenedores, namespace actual, cada namespace exterior, alias, namespaces
// importados, global, referenciados--. Esa escala EXISTIA, escrita una sola vez, dentro de
// `ProjectNameLookup`. El problema es que hay una segunda fase que necesita la misma respuesta y
// no puede preguntarle: la materializacion generica corre ANTES del modelo de nombres, porque
// tiene que crear los tipos que ese modelo va a declarar.
//
// Esa fase se escribio una version propia, mas simple: exacto, relativo al namespace del que
// llama, o sufijo. Y una version «parecida» de una regla de resolucion de nombres no es una
// aproximacion util, es un segundo lenguaje: dentro de `A.B`, `A.C` y `Z.C` son los dos sufijos
// de `.C`, de modo que la llamada se declaraba ambigua o clonaba la plantilla equivocada, mientras
// el modelo de nombres --el que manda-- decia sin dudar `A.C`.
//
// LO QUE SE COMPARTE ES EL ORDEN, NO LA BUSQUEDA. Cada fase tiene su propia tabla --tipos
// declarados una, plantillas genericas la otra-- y su propia nocion de visibilidad, y forzar a las
// dos por la misma funcion de sondeo cambiaria la resolucion de nombres de todo el arbol para
// arreglar un defecto que no esta ahi. Lo que estaba duplicado, y por tanto lo que se extrae, es
// la SECUENCIA DE ESCRITURAS CANDIDATAS: esta funcion la produce una vez y las dos fases la
// recorren en el mismo orden, cada una sondeando lo suyo.
//
// Que las dos coinciden de verdad no se afirma: hay una prueba diferencial que compara, para el
// mismo sitio y el mismo nombre escrito, el dueno que elige el modelo de nombres contra el que
// elige la escala generica.

// EL SITIO desde el que se escribe el nombre, en forma puramente TEXTUAL -- ni ordinales ni
// tablas. Es lo que permite que la fase generica, que no tiene modelo de nombres, construya el
// mismo sitio que el modelo de nombres construye con ordinales.
struct QualifiedNameSite final
{
    // El namespace lexico, de fuera adentro: {"A","B"} dentro de `namespace A { namespace B {`.
    std::span<const std::string> namespaceSegments;
    // La escritura COMPLETA Y CALIFICADA de cada tipo contenedor, de fuera adentro:
    // {"A.Outer", "A.Outer.Medio"} dentro de `A.Outer.Medio`. Completa y no simple porque el
    // escalon de contenedores produce nombres absolutos, no relativos.
    std::span<const std::string> enclosingTypeNames;
    // `using X = A.B.C;` -- solo la mitad de NAMESPACE. Un alias a un tipo lleva sintaxis, no una
    // cadena, y se expande antes de llegar aqui.
    const std::unordered_map<std::string, std::string>* aliases = nullptr;
    // `using A.B;`
    std::span<const std::string> importedNamespaces;
};

// A que escalon pertenece una escritura candidata. Cada fase sondea su propia tabla, y el escalon
// es lo que le dice CON QUE reglas de visibilidad sondearla.
enum class QualifiedNameTier
{
    // Un tipo contenedor del sitio. Solo el modulo propio.
    EnclosingType,
    // El namespace actual o uno exterior. Solo el modulo propio.
    LexicalNamespace,
    // Un `using` de namespace. El modulo propio y los referenciados, en el MISMO escalon: dos
    // candidatos iguales aqui son ambiguos, no se elige uno.
    ImportedNamespace,
    // El namespace global del modulo propio.
    Global,
    // Los modulos referenciados, por anidamiento de namespace y despues global.
    ReferencedModule
};

// UN ESCALON: las escrituras que compiten ENTRE SI. El primer escalon que produce algun resultado
// decide; dos resultados dentro del mismo escalon son ambiguos. Un escalon con una sola escritura
// --que es el caso normal-- es «prueba esta, y si esta no, baja».
struct QualifiedNameRung final
{
    QualifiedNameTier tier = QualifiedNameTier::LexicalNamespace;
    std::vector<std::string> spellings;
};

// Las escrituras candidatas, EN ORDEN DE PRIORIDAD. Sin tablas y sin decidir nada: quien decide es
// quien sondea.
std::vector<QualifiedNameRung> qualifiedNameLadder(
    const QualifiedNameSite& site, std::string_view writtenName);
}
