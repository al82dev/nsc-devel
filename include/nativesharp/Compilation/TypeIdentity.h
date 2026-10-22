#pragma once

#include "nativesharp/Compilation/TypeNameSegment.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
// NSC-LIMIT-009. EL FRAGMENTO DE SIMBOLO NATIVO de una clave de identidad, y por que tiene que
// ser una codificacion REVERSIBLE y no un saneador.
//
// Las identidades SI se distinguen. `Mod::N.Outer+Item` es el tipo anidado y `Mod::N.Outer.Item`
// es el tipo `Item` del namespace `N.Outer`. Pero las cuatro copias del saneador que habia en el
// arbol --HIR, MIR, destruccion y el lowerer-- aplastaban a `_` todo lo que no fuera alfanumerico,
// de modo que los dos producian el mismo simbolo: el mismo `__nsc_method_`, el mismo
// `__nsc_classdesc_`, el mismo `__nsc_destroy_` y la misma tabla de dispatch. Dos tipos, un cuerpo.
//
// LA PRIMERA CORRECCION FUE INSUFICIENTE, y conviene dejarlo escrito porque el error es facil de
// repetir: codificaba `+` como `$` y dejaba el resto igual. Eso arreglaba exactamente el par que
// la prueba medía y ninguno mas, porque `.` y `:` seguian yendo a `_`, que SI puede venir del
// usuario. Estos dos tipos, los dos legales, seguian colapsando:
//
//     Mod::A.B    ->  Mod__A_B      (clase `B` del namespace `A`)
//     Mod::A_B    ->  Mod__A_B      (clase llamada `A_B`)
//
// «`$` no puede venir del usuario» era cierto y no bastaba: la inyectividad la rompia `_`, no `$`.
//
// LA CODIFICACION. Alfanumerico ASCII y `_` se copian; CUALQUIER otro byte se escribe como
// `$` + dos digitos hexadecimales. Es reversible, y por tanto inyectiva, sin excepciones ni
// caracteres reservados que recordar: `.`, `:`, `+`, el acento grave de la aridad, los corchetes y
// lo que se anada despues a los `typeKey` quedan todos separados por construccion.
//
// EL `$` OBLIGA A LLVM A ENTRECOMILLAR EL NOMBRE, Y ESO HAY QUE SABERLO ANTES DE ESCRIBIR UNA
// BUSQUEDA SOBRE EL TEXTO DEL IR.
//
// Aqui decia que «`$` es un caracter ordinario en un simbolo COFF y en un identificador de LLVM».
// La primera mitad es cierta; la segunda no, y costo dieciocho comprobaciones rojas repartidas por
// nueve suites. `AsmWriter::PrintLLVMName` entrecomilla un nombre en cuanto contiene un byte que
// no sea alfanumerico, `-`, `.` o `_`. `$` no esta en esa lista. De modo que lo que sale impreso
// no es
//
//     define i32 @__nsc_method_Mod$3A$3AA$2EB__F(...)
// sino
//     define i32 @"__nsc_method_Mod$3A$3AA$2EB__F"(...)
//
// Y COMO `key()` ES SIEMPRE `Modulo::...`, el `::` esta siempre, su codificacion `$3A$3A` esta
// siempre, y por tanto TODO simbolo derivado de una identidad va SIEMPRE entrecomillado. No es una
// casualidad del conjunto actual de nombres: es un invariante, y por eso se puede escribir un
// ayudante en vez de recordarlo. Ver `irTextSymbolReference` mas abajo.
//
// QUE NO CAMBIA. El enlazador nunca ve las comillas --son de la representacion TEXTUAL, no del
// simbolo--, asi que ni el objeto, ni la tabla de simbolos, ni el mapa del enlazador, ni una
// biblioteca de importacion se enteran. Lo unico que cambia es quien LEE el texto del IR.
//
// QUIEN LEE EL TEXTO DEL IR, hoy: las pruebas, y el escaneo de imports de
// `createWindowsImportLibraries`. El escaneo busca cuatro nombres FIJOS --`@__C_specific_handler`,
// `@llvm.memset`, `@llvm.memcpy`, `@llvm.memmove`--, ninguno derivado de una identidad y ninguno
// con `$`, asi que hoy no le afecta. Eso es una propiedad del conjunto que busca, no una garantia:
// el dia que alguien anada ahi un simbolo NUESTRO tiene que pasar por `irTextSymbolReference`, o
// buscara una cadena que no existe y el fallo sera un import que falta, no un rojo.
//
// SOBRE EL CAMBIO DE FORMATO. Los nombres emitidos cambian --`Mod::A.B` pasa de `Mod__A_B` a
// `Mod$3A$3AA$2EB`-- y eso NO se disimula: todo simbolo derivado de una identidad lleva ahora
// `$3A$3A` (el `::` del modulo) y ninguno del formato anterior podia llevarlo. Un objeto viejo
// mezclado con uno nuevo falla en el enlazador con un externo no resuelto, que es exactamente el
// fallo que se quiere; no hay forma de que enlace en silencio contra el simbolo equivocado.
std::string symbolFragmentForIdentityKey(std::string_view identityKey);

// LA REGLA DE LLVM, escrita una vez. `AsmWriter::PrintLLVMName` entrecomilla si el nombre empieza
// por digito o si contiene un byte que no sea alfanumerico, `-`, `.` o `_`.
bool symbolNeedsIrTextQuotes(std::string_view symbolName) noexcept;

// EL NOMBRE TAL COMO APARECE EN EL TEXTO DEL IR, con su `@` y sus comillas si las lleva. Es lo que
// hay que buscar cuando se busca sobre `LlvmModule::text()`, y existe para que nadie tenga que
// acordarse de la regla de arriba: escribir `@__nsc_method_` a mano encuentra cero coincidencias y
// eso, en una asercion de AUSENCIA, es un verde falso.
std::string irTextSymbolReference(std::string_view symbolName);

// Y LO MISMO PARA UN PREFIJO. `@"__nsc_method_` no es un nombre completo y no se puede decidir con
// la regla de arriba --le falta el resto--, pero todo simbolo derivado de una identidad lleva el
// `$3A$3A` del `::` del modulo, asi que el prefijo va entrecomillado SIEMPRE. `prefix` es la parte
// literal, sin `@`: `irTextIdentitySymbolPrefix("__nsc_method_")` da `@"__nsc_method_`.
std::string irTextIdentitySymbolPrefix(std::string_view prefix);

class TypeIdentity final
{
public:
    TypeIdentity(
        std::string moduleName,
        std::vector<std::string> namespaceSegments,
        std::vector<TypeNameSegment> containingTypes,
        std::string name,
        std::uint32_t arity = 0);

    const std::string& moduleName() const noexcept;
    const std::vector<std::string>& namespaceSegments() const noexcept;
    const std::vector<TypeNameSegment>& containingTypes() const noexcept;
    const std::string& name() const noexcept;
    std::uint32_t arity() const noexcept;

    bool isValid() const noexcept;
    const std::string& validationError() const noexcept;
    std::string fullyQualifiedName() const;
    std::string key() const;
    std::string_view fullyQualifiedNameView() const noexcept;
    std::string_view keyView() const noexcept;

    bool operator==(const TypeIdentity& other) const noexcept;

private:
    static bool isIdentifier(std::string_view value) noexcept;
    static void appendTypeSegment(std::string& output, const TypeNameSegment& segment);
    void initializeNames();
    void validate();

    std::string moduleName_;
    std::vector<std::string> namespaceSegments_;
    std::vector<TypeNameSegment> containingTypes_;
    std::string name_;
    std::uint32_t arity_;
    std::string validationError_;
    std::string fullyQualifiedName_;
    std::string key_;
};
}
