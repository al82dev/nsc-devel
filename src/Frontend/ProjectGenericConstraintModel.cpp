#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
// El nombre corto de una clave de tipo, para el diagnostico. `type:App::App.Pieza` -> `Pieza`.
// El mensaje ya lleva el sitio; repetir la identidad canonica entera lo haria ilegible.
std::string shortName(std::string_view typeKey)
{
    std::string_view text = typeKey;
    if (text.starts_with("type:")) text.remove_prefix(5);
    if (const std::size_t module = text.find("::"); module != std::string_view::npos)
        text.remove_prefix(module + 2);
    if (const std::size_t dot = text.rfind('.'); dot != std::string_view::npos)
        text.remove_prefix(dot + 1);
    return std::string(text);
}

// Lo ESCRITO, para cuando el argumento no es un tipo declarado y no hay identidad que nombrar:
// `string`, `int[]`, `Pieza*`. Sin esto, un `Caja<string>` rechazado hablaria de una clave
// canonica que el autor no escribio en ninguna parte.
std::string writtenSpelling(const ast::TypeSyntax* syntax)
{
    if (syntax == nullptr) return "?";
    std::string text(syntax->baseName());
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        switch (modifier.kind)
        {
            case ast::TypeModifierSyntaxKind::Pointer: text += "*"; break;
            case ast::TypeModifierSyntaxKind::Array: text += "[N]"; break;
            case ast::TypeModifierSyntaxKind::DynamicArray: text += "[]"; break;
            case ast::TypeModifierSyntaxKind::RangeArray:
                text += '[' + std::string(
                    modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
                break;
            case ast::TypeModifierSyntaxKind::FlexibleArray: text += "[*]"; break;
        }
    }
    return text;
}

// La declaracion que una clave de tipo nombra, o null. Se busca por la clave y no por un indice
// prestado de otro modelo: los ordinales de `namedTypes()` son la identidad que comparten la
// herencia y las interfaces, asi que resolver aqui una vez evita tener que decidir a cual de los
// dos modelos preguntar segun lo que el tipo resulte ser.
const NamedTypeDeclaration* declarationForTypeKey(
    const ProjectNameLookup& nameLookup,
    std::string_view typeKey,
    std::size_t& ordinalOut)
{
    if (!typeKey.starts_with("type:")) return nullptr;
    const std::string_view identity = typeKey.substr(5);
    const std::span<const NamedTypeDeclaration> types = nameLookup.namedTypes();
    for (std::size_t index = 0; index < types.size(); ++index)
    {
        if (types[index].identity.key() != identity) continue;
        ordinalOut = index;
        return &types[index];
    }
    return nullptr;
}

// QUE ES UN TIPO REFERENCIA en este lenguaje, que es lo que `where T : class` promete.
//
// La primera version de esta comprobacion aceptaba clase e interfaz y nada mas, y era mas
// estrecha que el nombre que lleva: un DELEGADO es una referencia --tiene su propia
// NamedTypeDeclarationKind--, `string` lo es, y un ARREGLO lo es. Rechazarlos convertia
// `where T : class` en «where T : class or interface», que no es lo que la clausula dice.
//
// Un PUNTERO no entra, y no por descuido: `Pieza*` no participa del modelo de objetos --no tiene
// descriptor, no admite `is`, no lo recorre la destruccion-- y aceptarlo bajo `class` prometeria
// justamente eso. Se mira el modificador MAS EXTERNO, que es el que decide que es el valor:
// `Pieza*[]` es un arreglo (referencia) y `Pieza[]*` es un puntero.
bool isReferenceTypeArgument(
    const ast::TypeSyntax* syntax,
    const NamedTypeDeclaration* declaration)
{
    if (syntax == nullptr) return false;
    if (!syntax->modifiers().empty())
    {
        // SOLO EL ARREGLO DINAMICO. `T[]` es una referencia --un objeto con cabecera, longitud y
        // vida propia--; `T[N]` es ALMACENAMIENTO INLINE, tantos bytes seguidos dentro de quien lo
        // declara, sin identidad ni indireccion. Este mismo compilador los separa asi en
        // ProjectMethodBodyModel, y meter los dos bajo `class` prometia sobre `int[4]` lo que solo
        // es cierto de `int[]`.
        const ast::TypeModifierSyntaxKind outermost = syntax->modifiers().back().kind;
        return outermost == ast::TypeModifierSyntaxKind::DynamicArray ||
            outermost == ast::TypeModifierSyntaxKind::RangeArray;
    }
    if (syntax->baseTokenKind() == lexer::TokenKind::KwString) return true;
    if (declaration == nullptr) return false;
    return declaration->kind == NamedTypeDeclarationKind::Class ||
        declaration->kind == NamedTypeDeclarationKind::Interface ||
        declaration->kind == NamedTypeDeclarationKind::Delegate;
}

// Un diagnostico ya redactado que TODAVIA no se emitio. Existe por las sobrecargas: cuando varias
// plantillas comparten nombre y aridad se materializan todas, y que una incumpla no dice nada del
// programa hasta saber que ninguna otra servia.
struct PendingDiagnostic final
{
    diagnostics::DiagnosticId id = diagnostics::DiagnosticId::InvalidType;
    SourceRange range;
    std::string message;
    GenericConstraintViolation violation;
};
}

void ProjectGenericConstraintModel::validateTemplateClauses(
    const ProjectGenericModel& genericModel,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // LO QUE SE COMPRUEBA SOBRE LA PLANTILLA, y por que no sobre la especializacion.
    //
    // `where T : MiEnum` esta mal ESCRITO. No depende de con que se cierre `T`, no depende de que
    // alguien instancie, y el sitio que hay que corregir es la declaracion. Comprobarlo en la
    // especializacion --como hacia la primera version-- lo ataba a que la plantilla se usara: una
    // biblioteca con la clausula mal escrita y sin un solo uso salia verde.
    const std::span<const GenericTemplateRecord> templates = genericModel.templates();
    for (std::size_t templateOrdinal = 0; templateOrdinal < templates.size(); ++templateOrdinal)
    {
        const GenericTemplateRecord& templateRecord = templates[templateOrdinal];
        for (const ast::GenericConstraintClause& clause : templateRecord.constraints)
        {
            // Identidad -> primera escritura que la nombro. El parser ya rechaza dos veces el
            // MISMO texto; esto coge las dos escrituras DISTINTAS del mismo tipo --un alias, un
            // nombre corto frente a uno calificado-- que alli eran indistinguibles, porque
            // comparar nombres sin resolver es lo unico que el parser puede hacer.
            std::unordered_map<std::string, std::string> seenByKey;
            // Las clases base encontradas, EN ORDEN DE ESCRITURA, y cuantos constraints validos
            // las precedieron. Con eso se responden las tres preguntas de la regla de constraint
            // primario, que es la que faltaba entera.
            std::vector<std::pair<const ast::TypeSyntax*, std::size_t>> baseClasses;
            std::size_t acceptedConstraints = 0;
            for (const ast::TypeSyntax* constraintSyntax : clause.typeConstraints)
            {
                if (constraintSyntax == nullptr) continue;
                const std::string constraintKey =
                    memberModel.canonicalTypeKey(constraintSyntax, nameLookup);
                std::size_t constraintOrdinal = 0;
                const NamedTypeDeclaration* constraint =
                    declarationForTypeKey(nameLookup, constraintKey, constraintOrdinal);

                if (constraint == nullptr ||
                    (constraint->kind != NamedTypeDeclarationKind::Class &&
                     constraint->kind != NamedTypeDeclarationKind::Interface))
                {
                    violations_.push_back(GenericConstraintViolation{
                        GenericConstraintViolationKind::ConstraintIsNotAClassOrInterface,
                        templateOrdinal,
                        clause.parameterName,
                        std::string(),
                        constraintKey,
                        true});
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        constraintSyntax->range(),
                        "'" + std::string(constraintSyntax->baseName()) +
                            "' cannot constrain '" + clause.parameterName +
                            "': a generic constraint names a base class or an interface");
                    continue;
                }

                const auto previous = seenByKey.find(constraintKey);
                if (previous != seenByKey.end())
                {
                    violations_.push_back(GenericConstraintViolation{
                        GenericConstraintViolationKind::DuplicateConstraint,
                        templateOrdinal,
                        clause.parameterName,
                        std::string(),
                        constraintKey,
                        true});
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        constraintSyntax->range(),
                        "'" + std::string(constraintSyntax->baseName()) + "' and '" +
                            previous->second + "' name the same type '" +
                            shortName(constraintKey) + "'; '" + clause.parameterName +
                            "' is already constrained by it");
                    continue;
                }
                seenByKey.emplace(constraintKey, std::string(constraintSyntax->baseName()));
                if (constraint->kind == NamedTypeDeclarationKind::Class)
                {
                    baseClasses.emplace_back(constraintSyntax, acceptedConstraints);
                }
                ++acceptedConstraints;
            }

            // LA REGLA DE CONSTRAINT PRIMARIO, que es la de C# y no es cosmetica.
            //
            // Una clausula dice DOS cosas de distinta naturaleza: QUE ES el parametro --a lo sumo
            // una: `class`, o una clase base concreta-- y QUE IMPLEMENTA, que pueden ser varias.
            // La primera es unica porque no hay herencia multiple: dos clases base no describen
            // ningun tipo posible, y `class` mas una clase base es redundante en el mejor caso y
            // contradictorio en cuanto la base sea un struct. Aceptarlas dejaba escribir clausulas
            // que no restringen nada y que el lector interpreta como si restringieran.
            //
            // Y va PRIMERA por la misma razon que `class`: la lista se lee de izquierda a derecha
            // como «esto es un X que implementa Y y Z». Con la base en medio deja de leerse.
            if (clause.requiresClass && !baseClasses.empty())
            {
                reportPrimaryConstraint(
                    diagnostics,
                    templateOrdinal,
                    clause,
                    baseClasses.front().first,
                    "'class' and the base class '" +
                        std::string(baseClasses.front().first->baseName()) +
                        "' are both primary constraints of '" + clause.parameterName +
                        "'; a clause carries at most one");
            }
            else if (baseClasses.size() > 1)
            {
                reportPrimaryConstraint(
                    diagnostics,
                    templateOrdinal,
                    clause,
                    baseClasses[1].first,
                    "'" + clause.parameterName + "' cannot be constrained by two base classes ('" +
                        std::string(baseClasses.front().first->baseName()) + "' and '" +
                        std::string(baseClasses[1].first->baseName()) +
                        "'); this language has no multiple inheritance");
            }
            else if (!baseClasses.empty() && baseClasses.front().second != 0)
            {
                reportPrimaryConstraint(
                    diagnostics,
                    templateOrdinal,
                    clause,
                    baseClasses.front().first,
                    "the base class '" + std::string(baseClasses.front().first->baseName()) +
                        "' must be the first constraint of the 'where' clause for '" +
                        clause.parameterName + "', before any interface");
            }
        }
    }
}

void ProjectGenericConstraintModel::reportPrimaryConstraint(
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t templateOrdinal,
    const ast::GenericConstraintClause& clause,
    const ast::TypeSyntax* offending,
    const std::string& message)
{
    violations_.push_back(GenericConstraintViolation{
        GenericConstraintViolationKind::InvalidPrimaryConstraint,
        templateOrdinal,
        clause.parameterName,
        std::string(),
        offending != nullptr ? std::string(offending->baseName()) : std::string(),
        true});
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        offending != nullptr ? offending->range() : clause.range,
        message);
}

bool ProjectGenericConstraintModel::build(
    const ProjectGenericModel& genericModel,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    violations_.clear();
    invalidSpecializations_.clear();
    invalidTemplateOrdinals_.clear();
    built_ = true;

    // PRIMERA PASADA: lo que esta mal escrito en la declaracion, se instancie o no.
    validateTemplateClauses(genericModel, nameLookup, memberModel, diagnostics);

    // UNA PLANTILLA MAL DECLARADA INVALIDA A TODAS SUS ESPECIALIZACIONES, y eso se sabe AQUI, no
    // en la segunda pasada: no depende de ningun argumento. Se recolecta de `violations_` porque
    // toda violacion de declaracion guarda ahi el ordinal de la PLANTILLA -- ver el comentario
    // de `invalidTemplateOrdinals_`.
    for (const GenericConstraintViolation& violation : violations_)
    {
        if (violation.isDeclarationViolation)
        {
            invalidTemplateOrdinals_.insert(violation.specializationOrdinal);
        }
    }

    // SEGUNDA PASADA: lo que un ARGUMENTO concreto incumple. Se acumula sin emitir, porque una
    // especializacion puede ser una candidata descartada de un conjunto de sobrecargas.
    const std::span<const GenericTemplateRecord> templates = genericModel.templates();
    std::vector<PendingDiagnostic> ungrouped;

    for (const GenericSpecializationRecord& specialization : genericModel.specializations())
    {
        if (specialization.templateOrdinal >= templates.size()) continue;
        const GenericTemplateRecord& templateRecord = templates[specialization.templateOrdinal];
        if (templateRecord.constraints.empty() || specialization.specializedSyntax == nullptr)
        {
            continue;
        }
        // LA PLANTILLA YA ERA INVALIDA ANTES DE QUE ESTA ESPECIALIZACION EXISTIERA. Se marca y se
        // sale SIN emitir nada: el error ya salio sobre el `where`, que es donde hay que
        // corregirlo. Repetirlo aqui, una vez por instanciacion, seria el muro de errores de
        // plantilla que esta fase existe para quitar.
        if (invalidTemplateOrdinals_.contains(specialization.templateOrdinal))
        {
            invalidSpecializations_.insert(specialization.specializedSyntax);
            continue;
        }

        std::vector<PendingDiagnostic> pending;
        const std::string templateModuleSuffix =
            "; template is declared in module '" + templateRecord.moduleName + "'";

        // SE LEEN LAS CLAUSULAS DEL NODO ESPECIALIZADO, no las de la plantilla, y esa es la pieza
        // que hace innecesaria una segunda resolucion de nombres: el clon esta EN EL ARBOL, asi
        // que la resolucion ordinaria ya visito sus tipos de constraint y el argumento que cada
        // clausula lleva colgado. La plantilla solo aporta su nombre, para el mensaje.
        for (const ast::GenericConstraintClause& clause :
             specialization.specializedSyntax->genericConstraints())
        {
            // El argumento con el que se cerro este parametro. Falta solo si la sustitucion no lo
            // tenia, que el parser ya habria reportado como parametro desconocido: se sigue en
            // silencio en vez de encadenar un segundo diagnostico sobre la misma causa.
            const ast::TypeSyntax* argumentSyntax = clause.substitutedArgument;
            if (argumentSyntax == nullptr) continue;
            const std::string argumentKey =
                memberModel.canonicalTypeKey(argumentSyntax, nameLookup);

            std::size_t argumentOrdinal = 0;
            const NamedTypeDeclaration* argument =
                declarationForTypeKey(nameLookup, argumentKey, argumentOrdinal);
            const std::string argumentName = argument != nullptr
                ? shortName(argumentKey)
                : writtenSpelling(argumentSyntax);

            // `class`: el argumento tiene que ser un tipo REFERENCIA -- clase, interfaz, delegado,
            // `string` o arreglo. El calificador de propiedad del uso no entra aqui y no se toca:
            // `where T : class` habla de que ES T, no de quien lo posee.
            if (clause.requiresClass && !isReferenceTypeArgument(argumentSyntax, argument))
            {
                pending.push_back(PendingDiagnostic{
                    diagnostics::DiagnosticId::InvalidType,
                    specialization.instantiationRange,
                    "generic parameter '" + clause.parameterName + "' of '" +
                        templateRecord.name + "' requires a reference type ('where " +
                        clause.parameterName + " : class'), but the argument '" + argumentName +
                        "' is not a class, interface, delegate, string or dynamic array" +
                        templateModuleSuffix,
                    GenericConstraintViolation{
                        GenericConstraintViolationKind::RequiresReferenceType,
                        specialization.cacheOrdinal,
                        clause.parameterName,
                        argumentKey,
                        "class",
                        false}});
            }

            for (const ast::TypeSyntax* constraintSyntax : clause.typeConstraints)
            {
                if (constraintSyntax == nullptr) continue;
                const std::string constraintKey =
                    memberModel.canonicalTypeKey(constraintSyntax, nameLookup);
                std::size_t constraintOrdinal = 0;
                const NamedTypeDeclaration* constraint =
                    declarationForTypeKey(nameLookup, constraintKey, constraintOrdinal);

                // Un constraint que no nombra clase ni interfaz ya lo denuncio la primera pasada,
                // sobre la plantilla y una sola vez. Aqui se salta: repetirlo por cada `Caja<X>`
                // del programa llenaria la salida de copias del mismo error de declaracion.
                if (constraint == nullptr ||
                    (constraint->kind != NamedTypeDeclarationKind::Class &&
                     constraint->kind != NamedTypeDeclarationKind::Interface))
                {
                    continue;
                }

                if (argument == nullptr)
                {
                    // El argumento no nombra un tipo declarado -- un `int`, un `string`, un
                    // arreglo, un puntero. Ninguno deriva de nada ni implementa nada.
                    pending.push_back(PendingDiagnostic{
                        diagnostics::DiagnosticId::InvalidType,
                        specialization.instantiationRange,
                        "generic parameter '" + clause.parameterName + "' of '" +
                            templateRecord.name + "' requires '" + shortName(constraintKey) +
                            "', but the argument '" + argumentName + "' does not satisfy it" +
                            templateModuleSuffix,
                        GenericConstraintViolation{
                            constraint->kind == NamedTypeDeclarationKind::Interface
                                ? GenericConstraintViolationKind::DoesNotImplementInterface
                                : GenericConstraintViolationKind::DoesNotDeriveFromBase,
                            specialization.cacheOrdinal,
                            clause.parameterName,
                            argumentKey,
                            constraintKey,
                            false}});
                    continue;
                }

                bool satisfied = false;
                if (constraint->kind == NamedTypeDeclarationKind::Interface)
                {
                    // Dos caminos, y hacen falta los dos: una CLASE la implementa, y una INTERFAZ
                    // la extiende. `includeIdentity` es true en el segundo porque `where T : IFoo`
                    // lo satisface `IFoo` misma.
                    satisfied =
                        (argument->kind == NamedTypeDeclarationKind::Class &&
                         interfaceModel.classImplementsInterface(argumentOrdinal, constraintOrdinal)) ||
                        (argument->kind == NamedTypeDeclarationKind::Interface &&
                         interfaceModel.interfaceDerivesFrom(argumentOrdinal, constraintOrdinal, true));
                }
                else
                {
                    // Base class. La identidad cuenta: `where T : Base` lo satisface `Base`.
                    satisfied = inheritanceModel.isDerivedFrom(argumentOrdinal, constraintOrdinal, true);
                }

                if (!satisfied)
                {
                    pending.push_back(PendingDiagnostic{
                        diagnostics::DiagnosticId::InvalidType,
                        specialization.instantiationRange,
                        "generic parameter '" + clause.parameterName + "' of '" +
                            templateRecord.name + "' requires " +
                            (constraint->kind == NamedTypeDeclarationKind::Interface
                                 ? "an implementation of '"
                                 : "a type derived from '") +
                            shortName(constraintKey) + "', but the argument '" + argumentName +
                            "' " +
                             (constraint->kind == NamedTypeDeclarationKind::Interface
                                  ? "does not implement it"
                                  : "does not derive from it") + templateModuleSuffix,
                        GenericConstraintViolation{
                            constraint->kind == NamedTypeDeclarationKind::Interface
                                ? GenericConstraintViolationKind::DoesNotImplementInterface
                                : GenericConstraintViolationKind::DoesNotDeriveFromBase,
                            specialization.cacheOrdinal,
                            clause.parameterName,
                            argumentKey,
                            constraintKey,
                            false}});
                }
            }
        }

        if (pending.empty())
        {
            continue;
        }
        // Invalida es invalida: se marca SIEMPRE, antes de decidir si su diagnostico se aplaza.
        invalidSpecializations_.insert(specialization.specializedSyntax);
        if (specialization.overloadGroup == 0)
        {
            // No forma grupo: es la unica respuesta posible a su sitio, asi que su violacion es
            // el error del programa y se emite.
            ungrouped.insert(ungrouped.end(), pending.begin(), pending.end());
            continue;
        }
        // Forma grupo: se GUARDA y no se dice nada. Quien decide si esta candidata es la que el
        // programa usa es la resolucion de sobrecargas, que conoce los tipos de los argumentos.
        std::vector<DeferredDiagnostic>& deferred =
            deferredBySpecialization_[specialization.specializedSyntax];
        for (const PendingDiagnostic& entry : pending)
        {
            deferred.push_back(
                DeferredDiagnostic{entry.id, entry.range, entry.message, entry.violation});
        }
    }

    const auto emit = [&](const PendingDiagnostic& entry)
    {
        violations_.push_back(entry.violation);
        diagnostics.report(
            entry.id,
            diagnostics::DiagnosticSeverity::Error,
            entry.range,
            entry.message);
    };

    for (const PendingDiagnostic& entry : ungrouped)
    {
        emit(entry);
    }

    // Lo agrupado NO se emite aqui. Vive en `deferredBySpecialization_` hasta que la resolucion
    // de sobrecargas pregunte por una candidata concreta. Por eso el valor de retorno mira solo
    // lo emitido: decir «hay violaciones» de algo que quiza ningun programa usa seria mentir.
    return violations_.empty();
}

bool ProjectGenericConstraintModel::specializationSatisfiesConstraints(
    const ast::AstNode* specializedSyntax) const noexcept
{
    if (specializedSyntax == nullptr) return true;
    return !invalidSpecializations_.contains(specializedSyntax);
}

void ProjectGenericConstraintModel::reportSpecializationConstraintViolations(
    const ast::AstNode* specializedSyntax,
    SourceRange callRange,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (specializedSyntax == nullptr) return;
    const auto entry = deferredBySpecialization_.find(specializedSyntax);
    if (entry == deferredBySpecialization_.end()) return;
    for (const DeferredDiagnostic& deferred : entry->second)
    {
        // El rango del sitio manda sobre el guardado, que es el de la PRIMERA instanciacion: la
        // especializacion se cachea por clave, asi que una segunda llamada con el mismo argumento
        // no lo actualiza. Reportar el guardado apuntaria a una linea que ya compilaba.
        diagnostics.report(
            deferred.id, diagnostics::DiagnosticSeverity::Error, callRange, deferred.message);
    }
}

std::span<const GenericConstraintViolation> ProjectGenericConstraintModel::violations() const noexcept
{
    return violations_;
}

bool ProjectGenericConstraintModel::isBuilt() const noexcept
{
    return built_;
}
}
