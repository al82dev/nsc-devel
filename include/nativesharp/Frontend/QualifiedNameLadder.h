#pragma once

#include "nativesharp/Frontend/QualifiedNameSite.h"
#include "nativesharp/Frontend/QualifiedNameTier.h"
#include "nativesharp/Frontend/QualifiedNameRung.h"
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

// A que escalon pertenece una escritura candidata. Cada fase sondea su propia tabla, y el escalon
// es lo que le dice CON QUE reglas de visibilidad sondearla.

// UN ESCALON: las escrituras que compiten ENTRE SI. El primer escalon que produce algun resultado
// decide; dos resultados dentro del mismo escalon son ambiguos. Un escalon con una sola escritura
// --que es el caso normal-- es «prueba esta, y si esta no, baja».

// Las escrituras candidatas, EN ORDEN DE PRIORIDAD. Sin tablas y sin decidir nada: quien decide es
// quien sondea.
std::vector<QualifiedNameRung> qualifiedNameLadder(
    const QualifiedNameSite& site, std::string_view writtenName);
}
