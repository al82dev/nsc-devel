#include "nativesharp/Frontend/ClassLocalScalarField.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include <unordered_map>
#include <unordered_set>

// EN: Deferred inference of storage-preserving callees for the borrowed-view call rule
//     (docs/fixes/llamadas-preservadoras-de-almacenamiento.md). A view borrowed from an object stays valid while
//     nothing releases the storage it reads. A callee is storage-preserving when its body, proven by a
//     whitelist walk over its own bindings, can neither release memory nor call something that might: no
//     delete/destroy/placement, no lambda, assembly, using, foreach, move or static local, no class or
//     dropping creation, no unresolved, interface, virtual or precompiled call (readonly struct methods keep
//     the rule that already trusted them), no owned or dropping signature, and every write to storage that
//     is not a callee local or by-value parameter stores a type that releases nothing.
// ES: Inferencia diferida de callees que preservan almacenamiento para la regla de llamadas con vista
//     prestada. Una vista tomada de un objeto sigue valida mientras nada libere la memoria que lee. Un callee
//     preserva almacenamiento cuando su cuerpo, demostrado por un recorrido de lista blanca sobre sus propios
//     bindings, no puede liberar memoria ni llamar algo que pueda hacerlo: sin delete/destroy/placement,
//     lambdas, ensamblador, using, foreach, move ni locales estaticos; sin creaciones de clase o con drop; sin
//     llamadas no resueltas, de interfaz, virtuales o precompiladas (los metodos readonly de struct conservan
//     la regla que ya confiaba en ellos); sin firmas owned o con drop; y toda escritura a almacenamiento que no
//     sea un local del callee o un parametro por valor guarda un tipo que no libera nada.
// No recursion: the walk and the fixpoint use explicit worklists.
// Sin recursion: el recorrido y el punto fijo usan worklists explicitas.

namespace nativesharp::frontend
{

// EN: Records a borrowed-view site instead of reporting it when the callee is a candidate. Returns false (the
//     caller reports at once, as before) for any other callee. Effects: deferredStorageViewCalls_.
// ES: Registra un sitio con vista prestada en lugar de reportarlo cuando el callee es candidato. Devuelve false
//     (el llamador reporta de inmediato, como antes) para cualquier otro callee. Efectos: la lista diferida.
// Complexity: O(1).
bool ProjectMethodBodyModel::deferStorageViewCall(std::size_t calleeOrdinal, SourceRange range, std::string message)
{
    if (!storageViewCalleeIsCandidate(calleeOrdinal)) return false;
    deferredStorageViewCalls_.push_back({calleeOrdinal, range, std::move(message)});
    return true;
}

// EN: A callee whose body this build can inspect: source, concrete, non-virtual, not extern or imported, not
//     async or a generator, a method or constructor with a body.
// ES: Un callee cuyo cuerpo este build puede inspeccionar: fuente, concreto, no virtual, ni extern ni importado,
//     ni async ni generador, metodo o constructor con cuerpo.
// Complexity: O(1). Effects: none.
bool ProjectMethodBodyModel::storageViewCalleeIsCandidate(std::size_t calleeOrdinal) const
{
    const ClassMemberSymbol* callee = methodSymbol(calleeOrdinal);
    return callee != nullptr && memberModel_ != nullptr && !callee->isPrecompiled && !callee->isVirtual &&
        !callee->isOverride && !callee->isAbstract && !callee->isExternC && !callee->isImportedC &&
        (callee->kind == ClassMemberKind::Method || callee->kind == ClassMemberKind::Constructor) &&
        callee->syntax != nullptr && callee->syntax->first() != nullptr && !classLocalCallableMaySuspend(*callee);
}

// EN: No owned or dropping result or parameter: such a value would be released inside or after the call.
// ES: Sin resultado ni parametro owned o con drop: ese valor se liberaria dentro o despues de la llamada.
// Complexity: O(parameters). Effects: none.
bool ProjectMethodBodyModel::storageViewSignaturePreserves(const ClassMemberSymbol& callee) const
{
    if (callee.declaredOwnership == sema::OwnershipKind::Owned ||
        memberModel_->typeRequiresValueDrop(callee.declaredTypeKey)) return false;
    for (const MethodParameterSymbol& parameter : callee.parameters)
    {
        if (parameter.ownership == sema::OwnershipKind::Owned ||
            memberModel_->typeRequiresValueDrop(parameter.typeKey)) return false;
    }
    return true;
}

// EN: True when the named type declares an implicit user conversion, which may run user code invisibly.
// ES: Verdadero cuando el tipo nombrado declara una conversion implicita de usuario, que ejecuta codigo oculto.
// Complexity: O(members of the type). Effects: none.
bool ProjectMethodBodyModel::storageViewTypeDeclaresImplicitConversion(std::string_view typeKey) const
{
    const std::optional<std::size_t> type = namedTypeOrdinalForTypeKey(typeKey);
    if (!type.has_value()) return false;
    for (const std::size_t ordinal : memberModel_->membersOfType(*type))
    {
        if (ordinal < memberModel_->members().size() && memberModel_->members()[ordinal].isImplicitUserConversion)
            return true;
    }
    return false;
}

// EN: True when storing `value` into a `targetTypeKey` slot may go through an implicit user conversion.
// ES: Verdadero cuando guardar `value` en un lugar `targetTypeKey` puede pasar por una conversion implicita.
// Complexity: O(members of both types). Effects: none.
bool ProjectMethodBodyModel::storageViewMayConvertByUser(
    const ast::AstNode* value, std::string_view targetTypeKey, std::size_t calleeOrdinal) const
{
    const MethodExpressionTypeBinding* type = value != nullptr ? expressionTypeBinding(value, calleeOrdinal) : nullptr;
    if (type == nullptr || targetTypeKey.empty() || type->typeKey == targetTypeKey) return false;
    return storageViewTypeDeclaresImplicitConversion(type->typeKey) ||
        storageViewTypeDeclaresImplicitConversion(targetTypeKey);
}

// EN: A call (or getter/operator binding) inside a candidate body: resolved, without default arguments or
//     user conversions, with a preserving signature, and either a candidate (added to `dependencies`) or a
//     readonly struct instance method.
// ES: Una llamada (o binding de getter/operador) dentro de un cuerpo candidato: resuelta, sin argumentos por
//     defecto ni conversiones de usuario, con firma que preserva, y candidata (se agrega a `dependencies`) o
//     metodo de instancia readonly de struct.
// Complexity: O(arguments + parameters). Effects: `dependencies`.
bool ProjectMethodBodyModel::storageViewCallPreserves(
    const MethodCallBinding& call, std::vector<std::size_t>& dependencies) const
{
    if (call.resolution != MethodCallResolutionKind::Resolved || !call.selectedMemberOrdinal.has_value() ||
        !call.selectedDefaultArguments.empty()) return false;
    for (const ProjectImplicitConversion& conversion : call.selectedConversions)
    {
        if (conversion.kind == ProjectConversionKind::UserDefinedImplicit ||
            conversion.kind == ProjectConversionKind::UserDefinedExplicit) return false;
    }
    const ClassMemberSymbol* target = methodSymbol(*call.selectedMemberOrdinal);
    if (target == nullptr || !storageViewSignaturePreserves(*target)) return false;
    if (!call.interfaceCall && storageViewCalleeIsCandidate(*call.selectedMemberOrdinal))
    {
        dependencies.push_back(*call.selectedMemberOrdinal);
        return true;
    }
    return target->declaringTypeIsStruct && target->isReadonly && !target->isStatic;
}

// EN: A write inside a candidate body. Callee locals and by-value parameters are its own storage; a
//     constructor may initialize its own fields; any other target (field, element, dereference, ref/out
//     parameter, static) must have a type whose old value releases nothing: plain data or a pointer.
// ES: Una escritura dentro de un cuerpo candidato. Locales y parametros por valor son almacenamiento propio; un
//     constructor puede inicializar sus campos; cualquier otro destino (campo, elemento, desreferencia,
//     parametro ref/out, estatico) debe tener un tipo cuyo valor anterior no libera nada: datos simples o puntero.
// Complexity: O(type depth). Effects: none.
bool ProjectMethodBodyModel::storageViewWritePreserves(
    const ast::AstNode* target, std::size_t calleeOrdinal, bool constructor) const
{
    if (target == nullptr) return false;
    if (const MethodNameBinding* binding = nameBinding(target, calleeOrdinal); binding != nullptr)
    {
        if (binding->kind == MethodNameBindingKind::Local && binding->localOrdinal.has_value() &&
            *binding->localOrdinal < locals_.size() && !locals_[*binding->localOrdinal].isReference) return true;
        const ClassMemberSymbol* callee = methodSymbol(calleeOrdinal);
        if (binding->kind == MethodNameBindingKind::Parameter && binding->parameterOrdinal.has_value() &&
            callee != nullptr && *binding->parameterOrdinal < callee->parameters.size() &&
            callee->parameters[*binding->parameterOrdinal].passKind == ParameterPassKind::Value) return true;
        if (constructor && (binding->kind == MethodNameBindingKind::ImplicitField ||
                            binding->kind == MethodNameBindingKind::ExplicitThisField)) return true;
    }
    const MethodExpressionTypeBinding* type = expressionTypeBinding(target, calleeOrdinal);
    return type != nullptr && (type->typeKey.ends_with("*") || memberModel_->typeIsPlainData(type->typeKey));
}

// EN: `new S(args)` of a struct without value drop, without allocator or initializer, whose selected
//     constructor (and static constructor, if any) is a candidate added to `dependencies`. A creation that
//     selects no constructor is accepted only for a type that declares none (zero initialization).
// ES: `new S(args)` de un struct sin drop de valor, sin asignador ni inicializador, cuyo constructor elegido
//     (y el estatico, si existe) es candidato y se agrega a `dependencies`. Una creacion sin constructor elegido
//     solo se acepta para un tipo que no declara ninguno (inicializacion en cero).
// Complexity: O(members of the type + arguments). Effects: `dependencies`.
bool ProjectMethodBodyModel::storageViewCreationPreserves(const ast::AstNode* creation, std::size_t calleeOrdinal,
    const ProjectConstructionModel* constructions, std::vector<std::size_t>& dependencies) const
{
    if (creation->first() != nullptr || creation->second() != nullptr || creation->third() != nullptr ||
        nameLookup_ == nullptr) return false;
    const std::optional<std::size_t> siteOrdinal = constructionSiteIndex_.latest(creation, calleeOrdinal);
    if (!siteOrdinal.has_value() || *siteOrdinal >= constructionSites_.size()) return false;
    const ProjectConstructionSite& site = constructionSites_[*siteOrdinal];
    if (site.kind != ProjectConstructionSiteKind::Object || !site.allocatorTypeKey.empty() ||
        !site.targetTypeOrdinal.has_value() || *site.targetTypeOrdinal >= nameLookup_->namedTypes().size() ||
        nameLookup_->namedTypes()[*site.targetTypeOrdinal].kind != NamedTypeDeclarationKind::Struct ||
        memberModel_->typeRequiresValueDrop(site.targetTypeKey)) return false;
    bool declaresConstructor = false;
    for (const std::size_t ordinal : memberModel_->membersOfType(*site.targetTypeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) return false;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Constructor || member.declaringTypeOrdinal != *site.targetTypeOrdinal)
            continue;
        if (!member.isStatic)
        {
            declaresConstructor = true;
            continue;
        }
        if (!storageViewCalleeIsCandidate(ordinal)) return false;
        dependencies.push_back(ordinal);
    }
    const std::optional<std::size_t> selected =
        constructions != nullptr ? constructions->previewConstructionConstructor(site) : std::nullopt;
    if (!selected.has_value()) return !declaresConstructor && creation->children().empty();
    const ClassMemberSymbol* constructor = methodSymbol(*selected);
    if (constructor == nullptr || !storageViewCalleeIsCandidate(*selected) ||
        !storageViewSignaturePreserves(*constructor)) return false;
    for (std::size_t index = 0; index < site.argumentTypes.size() && index < constructor->parameters.size(); ++index)
    {
        const std::string& argumentType = site.argumentTypes[index].typeKey;
        const std::string& parameterType = constructor->parameters[index].typeKey;
        if (argumentType != parameterType && (storageViewTypeDeclaresImplicitConversion(argumentType) ||
                                              storageViewTypeDeclaresImplicitConversion(parameterType))) return false;
    }
    dependencies.push_back(*selected);
    return true;
}

namespace
{
// EN: Node kinds a storage-preserving body may contain; every other kind rejects the callee.
// ES: Tipos de nodo que puede contener un cuerpo que preserva; cualquier otro rechaza al callee.
// Complexity: O(1). Effects: none.
bool storageViewNodeKindAllowed(ast::AstNodeKind kind)
{
    switch (kind)
    {
        case ast::AstNodeKind::BlockStatement:
        case ast::AstNodeKind::VariableDeclarationStatement:
        case ast::AstNodeKind::IfStatement:
        case ast::AstNodeKind::WhileStatement:
        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::ForStatement:
        case ast::AstNodeKind::SwitchStatement:
        case ast::AstNodeKind::SwitchSection:
        case ast::AstNodeKind::SwitchLabel:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ExpressionStatement:
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::BinaryExpression:
        case ast::AstNodeKind::ConditionalExpression:
        case ast::AstNodeKind::AssignmentExpression:
        case ast::AstNodeKind::CallExpression:
        case ast::AstNodeKind::IndexExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::PointerMemberExpression:
        case ast::AstNodeKind::PostfixExpression:
        case ast::AstNodeKind::CastExpression:
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        case ast::AstNodeKind::DefaultValueExpression:
        case ast::AstNodeKind::ObjectCreationExpression:
            return true;
        default:
            return false;
    }
}

// EN: True for an `++`/`--` unary or postfix node, which writes its operand.
// ES: Verdadero para un nodo unario o postfijo `++`/`--`, que escribe su operando.
// Complexity: O(1). Effects: none.
bool storageViewIsIncrement(const ast::AstNode* node)
{
    return (node->kind() == ast::AstNodeKind::UnaryExpression ||
            node->kind() == ast::AstNodeKind::PostfixExpression) &&
        (node->tokenKind() == lexer::TokenKind::PlusPlus || node->tokenKind() == lexer::TokenKind::MinusMinus);
}
}

// EN: Checks one node of a candidate body: an allowed kind, every call binding on it, no event, and the
//     write, creation and implicit-conversion rules for the node kinds that store a value.
// ES: Revisa un nodo de un cuerpo candidato: tipo permitido, todo binding de llamada en el, sin eventos, y las
//     reglas de escritura, creacion y conversion implicita para los nodos que guardan un valor.
// Complexity: O(1) plus the rule applied. Effects: `dependencies`.
bool ProjectMethodBodyModel::storageViewNodePreserves(const ast::AstNode* node, const ClassMemberSymbol& callee,
    std::size_t calleeOrdinal, const ProjectConstructionModel* constructions,
    std::vector<std::size_t>& dependencies) const
{
    if (!storageViewNodeKindAllowed(node->kind())) return false;
    if (const MethodCallBinding* call = callBinding(node, calleeOrdinal); call != nullptr)
    {
        if (!storageViewCallPreserves(*call, dependencies)) return false;
    }
    else if (node->kind() == ast::AstNodeKind::CallExpression) return false;
    if (const MethodNameBinding* name = nameBinding(node, calleeOrdinal); name != nullptr &&
        (name->kind == MethodNameBindingKind::ImplicitEvent || name->kind == MethodNameBindingKind::ExplicitThisEvent ||
         name->kind == MethodNameBindingKind::ExplicitObjectEvent || name->kind == MethodNameBindingKind::StaticEvent))
        return false;
    const bool constructor = callee.kind == ClassMemberKind::Constructor && !callee.isStatic;
    if (storageViewIsIncrement(node)) return storageViewWritePreserves(node->first(), calleeOrdinal, constructor);
    if (node->kind() == ast::AstNodeKind::AssignmentExpression)
    {
        const MethodExpressionTypeBinding* target = expressionTypeBinding(node->first(), calleeOrdinal);
        return storageViewWritePreserves(node->first(), calleeOrdinal, constructor) && target != nullptr &&
            !storageViewMayConvertByUser(node->second(), target->typeKey, calleeOrdinal);
    }
    if (node->kind() == ast::AstNodeKind::ObjectCreationExpression)
        return storageViewCreationPreserves(node, calleeOrdinal, constructions, dependencies);
    if (node->kind() == ast::AstNodeKind::ReturnStatement)
        return !storageViewMayConvertByUser(node->first(), callee.declaredTypeKey, calleeOrdinal);
    if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement && node->first() != nullptr)
    {
        if (node->typeSyntax() == nullptr || nameLookup_ == nullptr) return false;
        return !storageViewMayConvertByUser(
            node->first(), memberModel_->canonicalTypeKey(node->typeSyntax(), *nameLookup_), calleeOrdinal);
    }
    return true;
}

// EN: Whitelist walk over the body of a candidate (explicit stack). A constructor with a `this(...)`/`base(...)`
//     initializer is rejected. Callees and constructors the body relies on are appended to `dependencies`.
// ES: Recorrido de lista blanca sobre el cuerpo de un candidato (pila explicita). Se rechaza un constructor con
//     inicializador `this(...)`/`base(...)`. Los callees y constructores de los que depende van a `dependencies`.
// Complexity: O(body nodes). Effects: `dependencies`.
bool ProjectMethodBodyModel::storageViewCalleeBodyPreserves(std::size_t calleeOrdinal,
    const ProjectConstructionModel* constructions, std::vector<std::size_t>& dependencies) const
{
    const ClassMemberSymbol* callee = methodSymbol(calleeOrdinal);
    if (callee == nullptr || !storageViewSignaturePreserves(*callee) ||
        (callee->kind == ClassMemberKind::Constructor && callee->syntax->second() != nullptr)) return false;
    std::vector<const ast::AstNode*> pending{callee->syntax->first()};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        if (!storageViewNodePreserves(node, *callee, calleeOrdinal, constructions, dependencies)) return false;
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
    return true;
}

// EN: Greatest fixpoint over the callees reached from the deferred sites. Exploration (worklist) checks each
//     callee once and records reverse dependency edges; a callee that fails, whose body was not analyzed, or
//     that owns a releasing local (static, raii, scope, owned or dropping) is rejected; rejection then spreads
//     backwards to every dependent (second worklist). Cycles without a rejected member stay preserving. Each
//     deferred site whose callee is rejected reports its original diagnostic.
// ES: Punto fijo mayor sobre los callees alcanzados desde los sitios diferidos. La exploracion (worklist) revisa
//     cada callee una vez y guarda aristas inversas; se rechaza un callee que falla, cuyo cuerpo no se analizo o
//     que tiene un local que libera (estatico, raii, scope, owned o con drop); el rechazo se propaga hacia atras
//     a todo dependiente (segunda worklist). Un ciclo sin miembro rechazado sigue preservando. Cada sitio diferido
//     cuyo callee queda rechazado emite su diagnostico original.
// Complexity: O(locals + reached body nodes + edges). Effects: diagnostics.
void ProjectMethodBodyModel::finalizeStorageViewCalls(
    const ProjectConstructionModel* constructions, diagnostics::DiagnosticEngine& diagnostics) const
{
    if (deferredStorageViewCalls_.empty() || memberModel_ == nullptr) return;
    std::unordered_set<std::size_t> releasing;
    for (const MethodLocalSymbol& local : locals_)
    {
        if (local.isStatic || local.isRaii || local.isScope || local.ownership == sema::OwnershipKind::Owned ||
            memberModel_->typeRequiresValueDrop(local.typeKey)) releasing.insert(local.methodMemberOrdinal);
    }
    std::unordered_map<std::size_t, std::vector<std::size_t>> dependents;
    std::unordered_set<std::size_t> explored;
    std::unordered_set<std::size_t> rejected;
    std::vector<std::size_t> pending;
    for (const DeferredStorageViewCall& site : deferredStorageViewCalls_) pending.push_back(site.calleeOrdinal);
    std::vector<std::size_t> dependencies;
    while (!pending.empty())
    {
        const std::size_t callee = pending.back();
        pending.pop_back();
        if (!explored.insert(callee).second) continue;
        dependencies.clear();
        if (!callableBodyAnalyzed(callee) || releasing.contains(callee) || !storageViewCalleeIsCandidate(callee) ||
            !storageViewCalleeBodyPreserves(callee, constructions, dependencies))
        {
            rejected.insert(callee);
            continue;
        }
        for (const std::size_t dependency : dependencies)
        {
            dependents[dependency].push_back(callee);
            pending.push_back(dependency);
        }
    }
    std::vector<std::size_t> spread(rejected.begin(), rejected.end());
    while (!spread.empty())
    {
        const std::size_t callee = spread.back();
        spread.pop_back();
        const auto found = dependents.find(callee);
        if (found == dependents.end()) continue;
        for (const std::size_t dependent : found->second)
            if (rejected.insert(dependent).second) spread.push_back(dependent);
    }
    for (const DeferredStorageViewCall& site : deferredStorageViewCalls_)
    {
        if (rejected.contains(site.calleeOrdinal))
            diagnostics.report(diagnostics::DiagnosticId::InvalidCall, diagnostics::DiagnosticSeverity::Error,
                site.range, site.message);
    }
}

}
