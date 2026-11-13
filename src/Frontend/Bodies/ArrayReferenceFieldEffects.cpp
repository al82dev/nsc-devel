#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <limits>
#include <set>
#include <utility>
#include <vector>

// EN: Class references held by fields of a local receiver (docs/3.0/main-fixes/07, M2-06). The array
//     lifetime rule of documents 04 and 06 covered native arrays held by locals; a reference stored
//     through a method into a field array (or a plain reference field) of a local object, and used
//     later through another method of that object, was a route the replay never visited (probes P18
//     and P25). Each instance method gets a summary of what it does with its receiver's reference
//     fields; the replay of a caller turns each field of each tracked receiver object into a pseudo
//     array and applies the same life rule to the uses performed by the calls.
// ES: Referencias de clase guardadas en campos de un receptor local (docs/3.0/main-fixes/07, M2-06).
//     La regla de vida de los documentos 04 y 06 cubria arreglos nativos en locales; una referencia
//     guardada mediante un metodo en un arreglo en campo (o un campo de referencia simple) de un objeto
//     local, y usada despues mediante otro metodo de ese objeto, era una ruta que el replay nunca
//     visitaba (sondas P18 y P25). Cada metodo de instancia recibe un resumen de lo que hace con los
//     campos de referencia de su receptor; el replay de un llamador convierte cada campo de cada objeto
//     receptor rastreado en un pseudoarreglo y aplica la misma regla de vida a los usos que hacen las
//     llamadas.
namespace nativesharp::frontend
{
namespace
{
constexpr std::size_t kArrayReferenceUnknownReceiver = std::numeric_limits<std::size_t>::max();
constexpr std::size_t kArrayReferenceFieldArrayBase = ArrayReferenceFlowState::kFieldArrayBase;

bool isSequentialExit(const ast::AstNode* node)
{
    std::vector<const ast::AstNode*> pending{node};
    while (!pending.empty())
    {
        const ast::AstNode* current = pending.back();
        pending.pop_back();
        if (current == nullptr || current->kind() == ast::AstNodeKind::LambdaExpression) continue;
        const auto kind = current->kind();
        if (kind == ast::AstNodeKind::ReturnStatement || kind == ast::AstNodeKind::ThrowStatement ||
            kind == ast::AstNodeKind::BreakStatement || kind == ast::AstNodeKind::ContinueStatement)
            return true;
        pending.push_back(current->first());
        pending.push_back(current->second());
        pending.push_back(current->third());
        for (const ast::AstNode* child : current->children()) pending.push_back(child);
    }
    return false;
}
}

// EN: Summary of `callable` (an instance method of a source class). Only the shapes below are
//     modelled; anything else touching a reference field is Unknown for that field, and anything that
//     lets the receiver reach unmodelled code is UnknownAll.
//     - `F = p;` / `this.F = p;` and `F[i] = p;` with `p` a parameter: Store (index from a literal).
//     - `F` / `F[i]` as an operand of `==` / `!=`, `F.Length`: not a use.
//     - `F[i]` and a plain `F` in any other position: Use (index from a literal).
//     - an array field in any other position, or `F = ...` replacing an array field: Unknown + Use.
//     - a non-virtual call on `this`: the callee's summary, with Store parameters mapped through
//       direct parameter arguments; a virtual or unresolved call on `this`, a lambda, or `this` in any
//       other position: UnknownAll.
//     An effect is definite when it is in a top-level statement of a block body, outside `?:`, `&&`,
//     `||` and `??`, with no `return`, `throw`, `break` or `continue` in an earlier top-level statement.
// ES: Resumen de `callable` (un metodo de instancia de una clase fuente). Solo se modelan las formas de
//     arriba; cualquier otra que toque un campo de referencia es Unknown para ese campo, y cualquier cosa
//     que deje al receptor llegar a codigo no modelado es UnknownAll. Un efecto es cierto cuando esta
//     en una sentencia de primer nivel de un cuerpo de bloque, fuera de `?:`, `&&`, `||` y `??`, sin
//     `return`, `throw`, `break` ni `continue` en una sentencia de primer nivel anterior.
// Complexity: O(body nodes + summaries of the callees on `this`), memoized. Effects: the cache.
const std::vector<ArrayReferenceFieldEffect>& ProjectMethodBodyModel::arrayReferenceFieldEffects(
    std::size_t callable) const
{
    static const std::vector<ArrayReferenceFieldEffect> none;
    static const std::vector<ArrayReferenceFieldEffect> unknownAll{
        ArrayReferenceFieldEffect{ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false}};
    if (const auto found = arrayReferenceFieldEffects_.find(callable); found != arrayReferenceFieldEffects_.end())
        return found->second;
    if (arrayReferenceFieldEffectsInProgress_.contains(callable)) return unknownAll;
    const ClassMemberSymbol* symbol = methodSymbol(callable);
    if (symbol == nullptr || memberModel_ == nullptr || symbol->isStatic ||
        symbol->kind != ClassMemberKind::Method) return none;
    if (symbol->isPrecompiled || symbol->syntax == nullptr || symbol->syntax->first() == nullptr)
        return arrayReferenceFieldEffects_.emplace(callable, unknownAll).first->second;
    arrayReferenceFieldEffectsInProgress_.insert(callable);
    std::vector<ArrayReferenceFieldEffect> effects;
    const ast::AstNode* body = symbol->syntax->first();
    const std::size_t memberCount = memberModel_->members().size();
    const auto fieldOf = [&](const ast::AstNode* node) -> std::optional<std::pair<std::size_t, bool>>
    {
        if (node->kind() == ast::AstNodeKind::MemberExpression)
        {
            if (node->first() == nullptr || node->first()->kind() != ast::AstNodeKind::ThisExpression)
                return std::nullopt;
        }
        else if (node->kind() != ast::AstNodeKind::IdentifierExpression) return std::nullopt;
        const MethodNameBinding* binding = nameBinding(node, callable);
        if (binding == nullptr || (binding->kind != MethodNameBindingKind::ImplicitField &&
                                   binding->kind != MethodNameBindingKind::ExplicitThisField) ||
            binding->candidateMemberOrdinals.size() != 1 || binding->candidateMemberOrdinals[0] >= memberCount)
            return std::nullopt;
        const std::size_t ordinal = binding->candidateMemberOrdinals[0];
        const auto tracked = arrayReferenceTrackedField(ordinal);
        if (!tracked.has_value()) return std::nullopt;
        return std::pair<std::size_t, bool>{ordinal, *tracked};
    };
    const auto parameterOf = [&](const ast::AstNode* node) -> std::optional<std::size_t>
    {
        if (node == nullptr || node->kind() != ast::AstNodeKind::IdentifierExpression) return std::nullopt;
        const MethodNameBinding* binding = nameBinding(node, callable);
        return binding != nullptr && binding->kind == MethodNameBindingKind::Parameter
            ? binding->parameterOrdinal : std::nullopt;
    };
    const auto literalIndex = [](const ast::AstNode* index) -> std::optional<std::int64_t>
    {
        if (index == nullptr || index->kind() != ast::AstNodeKind::IntegerLiteralExpression ||
            index->integerValue() > 0x7fffffffffffffffULL) return std::nullopt;
        return static_cast<std::int64_t>(index->integerValue());
    };
    const auto isComparison = [](const ast::AstNode* node)
    {
        return node != nullptr && node->kind() == ast::AstNodeKind::BinaryExpression &&
            (node->tokenKind() == lexer::TokenKind::EqualEqual || node->tokenKind() == lexer::TokenKind::BangEqual);
    };
    const auto isConditional = [](const ast::AstNode* node)
    {
        if (node->kind() == ast::AstNodeKind::ConditionalExpression) return true;
        if (node->kind() != ast::AstNodeKind::BinaryExpression) return false;
        const auto token = node->tokenKind();
        return token == lexer::TokenKind::AmpAmp || token == lexer::TokenKind::PipePipe ||
            token == lexer::TokenKind::QuestionQuestion;
    };
    // EN: Top-level statements of a block body that run on every normal path from entry.
    // ES: Sentencias de primer nivel de un cuerpo de bloque que corren en todo camino normal.
    std::set<const ast::AstNode*> definiteStatements;
    if (body->kind() == ast::AstNodeKind::BlockStatement)
        for (const ast::AstNode* statement : body->children())
        {
            if (statement == nullptr) continue;
            definiteStatements.insert(statement);
            if (isSequentialExit(statement)) break;
        }
    struct Frame final
    {
        const ast::AstNode* node;
        std::vector<const ast::AstNode*> ancestors;
    };
    const auto definiteAt = [&](const std::vector<const ast::AstNode*>& ancestors)
    {
        if (ancestors.size() < 2 || ancestors[0] != body || !definiteStatements.contains(ancestors[1])) return false;
        for (std::size_t i = 2; i < ancestors.size(); ++i)
            if (isConditional(ancestors[i])) return false;
        return true;
    };
    std::vector<Frame> pending{{body, {}}};
    while (!pending.empty())
    {
        Frame frame = std::move(pending.back());
        pending.pop_back();
        const ast::AstNode* node = frame.node;
        if (node == nullptr) continue;
        const ast::AstNode* parent = frame.ancestors.empty() ? nullptr : frame.ancestors.back();
        const ast::AstNode* grand = frame.ancestors.size() < 2 ? nullptr : frame.ancestors[frame.ancestors.size() - 2];
        if (node->kind() == ast::AstNodeKind::LambdaExpression)
        {
            effects.push_back({ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false});
            continue;
        }
        if (node->kind() == ast::AstNodeKind::ThisExpression)
        {
            if (parent == nullptr || parent->kind() != ast::AstNodeKind::MemberExpression || parent->first() != node)
                effects.push_back({ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false});
            continue;
        }
        if (const auto field = fieldOf(node))
        {
            const bool definite = definiteAt(frame.ancestors);
            if (!field->second)
            {
                if (parent != nullptr && parent->kind() == ast::AstNodeKind::AssignmentExpression &&
                    parent->first() == node)
                {
                    if (parent->tokenKind() == lexer::TokenKind::Equal)
                        if (const auto parameter = parameterOf(parent->second()))
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Store, field->first, parameter, 0,
                                definiteAt(std::vector<const ast::AstNode*>(
                                    frame.ancestors.begin(), frame.ancestors.end() - 1))});
                }
                else if (!isComparison(parent))
                    effects.push_back({ArrayReferenceFieldEffect::Kind::Use, field->first, std::nullopt, 0, definite});
                continue;
            }
            if (parent != nullptr && parent->kind() == ast::AstNodeKind::IndexExpression && parent->first() == node)
            {
                const ast::AstNode* indexNode = parent->children().size() == 1
                    ? parent->children().front() : (parent->children().empty() ? parent->second() : nullptr);
                const auto index = literalIndex(indexNode);
                if (grand != nullptr && grand->kind() == ast::AstNodeKind::AssignmentExpression &&
                    grand->first() == parent)
                {
                    if (grand->tokenKind() == lexer::TokenKind::Equal)
                        if (const auto parameter = parameterOf(grand->second()))
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Store, field->first, parameter, index,
                                definiteAt(std::vector<const ast::AstNode*>(
                                    frame.ancestors.begin(), frame.ancestors.end() - 1))});
                }
                else if (!isComparison(grand))
                    effects.push_back({ArrayReferenceFieldEffect::Kind::Use, field->first, std::nullopt, index,
                        definiteAt(std::vector<const ast::AstNode*>(
                            frame.ancestors.begin(), frame.ancestors.end() - 1))});
                // EN: The index expression is still visited (it may use other fields).
                // ES: La expresion de indice se sigue visitando (puede usar otros campos).
                std::vector<const ast::AstNode*> ancestors = frame.ancestors;
                ancestors.push_back(parent);
                if (indexNode != nullptr) pending.push_back({indexNode, std::move(ancestors)});
                continue;
            }
            if (parent != nullptr && parent->kind() == ast::AstNodeKind::MemberExpression && parent->first() == node &&
                parent->text() == "Length") continue;
            effects.push_back(
                {ArrayReferenceFieldEffect::Kind::Unknown, field->first, std::nullopt, std::nullopt, false});
            effects.push_back({ArrayReferenceFieldEffect::Kind::Use, field->first, std::nullopt, std::nullopt, false});
            continue;
        }
        if (node->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCallBinding* call = callBinding(node, callable);
            const bool onThis = call != nullptr && !call->staticCall &&
                (call->receiverSyntax == nullptr || call->receiverSyntax->kind() == ast::AstNodeKind::ThisExpression);
            if (onThis)
            {
                const ClassMemberSymbol* target = call->resolution == MethodCallResolutionKind::Resolved &&
                    call->selectedMemberOrdinal.has_value() ? methodSymbol(*call->selectedMemberOrdinal) : nullptr;
                if (target == nullptr || target->isVirtual || target->isOverride || target->isAbstract)
                    effects.push_back(
                        {ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false});
                else
                {
                    const bool definite = definiteAt(frame.ancestors);
                    const std::vector<ArrayReferenceFieldEffect> inner =
                        arrayReferenceFieldEffects(*call->selectedMemberOrdinal);
                    for (ArrayReferenceFieldEffect effect : inner)
                    {
                        effect.definite = effect.definite && definite;
                        if (effect.kind == ArrayReferenceFieldEffect::Kind::Store)
                        {
                            std::optional<std::size_t> mapped;
                            for (std::size_t i = 0; i < call->selectedArgumentParameterOrdinals.size() &&
                                 i < call->selectedArgumentSyntaxes.size(); ++i)
                                if (call->selectedArgumentParameterOrdinals[i] == effect.sourceParameter)
                                    mapped = parameterOf(call->selectedArgumentSyntaxes[i]);
                            if (!mapped.has_value()) continue;
                            effect.sourceParameter = mapped;
                        }
                        effects.push_back(effect);
                    }
                }
            }
        }
        std::vector<const ast::AstNode*> ancestors = frame.ancestors;
        ancestors.push_back(node);
        for (const ast::AstNode* child : {node->first(), node->second(), node->third()})
            if (child != nullptr) pending.push_back({child, ancestors});
        for (const ast::AstNode* child : node->children())
            if (child != nullptr) pending.push_back({child, ancestors});
    }
    arrayReferenceFieldEffectsInProgress_.erase(callable);
    return arrayReferenceFieldEffects_.insert_or_assign(callable, std::move(effects)).first->second;
}

// EN: Whether a field holds class references the field routes track: nullopt when it does not, true
//     for a native array of class references, false for a plain class reference. Static and `owned`
//     fields are excluded (owned fields follow the ownership rules; statics are known-life limit 1).
// ES: Si un campo guarda referencias de clase que siguen las rutas de campo: nullopt si no, true para un
//     arreglo nativo de referencias de clase, false para una referencia de clase simple. Se excluyen los
//     campos static y `owned` (los owned siguen las reglas de ownership; los static son el limite 1 de
//     fuentes de vida conocida).
// Complexity: O(1). Effects: none.
std::optional<bool> ProjectMethodBodyModel::arrayReferenceTrackedField(std::size_t fieldOrdinal) const
{
    if (memberModel_ == nullptr || fieldOrdinal >= memberModel_->members().size()) return std::nullopt;
    const ClassMemberSymbol& field = memberModel_->members()[fieldOrdinal];
    if (field.kind != ClassMemberKind::Field || field.isStatic ||
        field.declaredOwnership == sema::OwnershipKind::Owned) return std::nullopt;
    if (ProjectTypeConversion::isNativeArray(field.declaredTypeKey))
    {
        const auto element = ProjectTypeConversion::nativeArrayElementType(field.declaredTypeKey);
        if (!element.has_value() || *element == "builtin:string" || !isReferenceTypeKey(*element))
            return std::nullopt;
        return true;
    }
    if (field.declaredTypeKey == "builtin:string" || !isReferenceTypeKey(field.declaredTypeKey)) return std::nullopt;
    return false;
}

// EN: Pseudo array for `field` of `receiverObject` in the current replay; a receiver that escaped, and
//     the unknown receiver (which collects every stored object), start escaped and unknown.
// ES: Pseudoarreglo para `field` de `receiverObject` en el replay actual; un receptor que escapo y el
//     receptor desconocido (que reune todo objeto guardado) empiezan escapados y desconocidos.
// Complexity: O(log field arrays). Effects: arrayReferenceFieldArrays_, arrayReferenceFlow_.
std::size_t ProjectMethodBodyModel::arrayReferenceFieldArray(std::size_t receiverObject, std::size_t field)
{
    const auto key = std::make_pair(receiverObject, field);
    if (const auto found = arrayReferenceFieldArrays_.find(key); found != arrayReferenceFieldArrays_.end())
        return found->second;
    const std::size_t id = kArrayReferenceFieldArrayBase + arrayReferenceFieldArrays_.size();
    arrayReferenceFieldArrays_.emplace(key, id);
    if (receiverObject == kArrayReferenceUnknownReceiver || arrayReferenceFieldAllUnknown_ ||
        arrayReferenceFieldUnknownFields_.contains(field) ||
        arrayReferenceFieldEscapedReceivers_.contains(receiverObject))
    {
        arrayReferenceFlow_.escapedArrays.insert(id);
        arrayReferenceFlow_.unknownArrays.insert(id);
    }
    return id;
}

// EN: Replay of one call for the field routes. A tracked owner passed as an argument lets the callee
//     reach its fields: its field arrays escape. On a local receiver, the callee's summary is applied
//     to the receiver object's pseudo arrays (a receiver that is not a single tracked owner uses the
//     unknown receiver): uses first (they read what was stored before the call), then stores. A call
//     without a summary (virtual, interface, unresolved) or with UnknownAll uses every field array of
//     the receiver at an unknown index and lets them escape. The uses of a call become one
//     observation on the call syntax, assessed by the life rule of document 04; a destroyed object is
//     reported as destroyed only when the callee uses it on every path.
// ES: Replay de una llamada para las rutas de campo. Un dueno rastreado pasado como argumento deja que
//     el callee alcance sus campos: sus arreglos en campo escapan. Sobre un receptor local se aplica el
//     resumen del callee a los pseudoarreglos del objeto receptor (un receptor que no es un unico dueno
//     rastreado usa el receptor desconocido): primero los usos (leen lo guardado antes de la llamada),
//     despues los guardados. Una llamada sin resumen (virtual, de interfaz, no resuelta) o con
//     UnknownAll usa todo arreglo en campo del receptor con indice desconocido y lo deja escapar. Los
//     usos de una llamada forman una observacion sobre la sintaxis de la llamada, evaluada con la regla
//     de vida del documento 04; un objeto destruido se reporta como destruido solo cuando el callee lo
//     usa en todo camino.
// Complexity: O(summary effects * log state). Effects: arrayReferenceFlow_, replay observations.
void ProjectMethodBodyModel::recordArrayReferenceFieldCall(
    const MethodCallBinding& binding, const ClassMemberSymbol* target)
{
    if (arrayReferenceReplaySource_ == nullptr) return;
    const auto localFor = [&](const ast::AstNode* argument) -> std::optional<std::size_t>
    {
        if (argument != nullptr && argument->kind() == ast::AstNodeKind::MoveExpression) argument = argument->first();
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) return std::nullopt;
        const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, argument->text());
        return name.has_value() && name->kind == ScopeNameKind::Local
            ? std::optional<std::size_t>(name->semanticOrdinal) : std::nullopt;
    };
    const auto objectsOf = [&](std::size_t local) -> const ArrayReferenceFlowState::IdSet*
    {
        const auto owner = arrayReferenceFlow_.owners.find(local);
        return owner == arrayReferenceFlow_.owners.end() ? nullptr : &owner->second;
    };
    const auto markAllFieldsUnknown = [&]
    {
        arrayReferenceFieldAllUnknown_ = true;
        for (const auto& [key, id] : arrayReferenceFieldArrays_)
        {
            arrayReferenceFlow_.escapedArrays.insert(id);
            arrayReferenceFlow_.unknownArrays.insert(id);
        }
    };
    const auto markFieldUnknown = [&](std::size_t field, const ArrayReferenceFlowState::IdSet& values)
    {
        arrayReferenceFieldUnknownFields_.insert(field);
        arrayReferenceFlow_.exposedObjects.insert(values.begin(), values.end());
        for (const auto& [key, id] : arrayReferenceFieldArrays_)
            if (key.second == field)
            {
                arrayReferenceFlow_.escapedArrays.insert(id);
                arrayReferenceFlow_.unknownArrays.insert(id);
            }
    };
    const auto escapeReceiver = [&](std::size_t object)
    {
        arrayReferenceFieldEscapedReceivers_.insert(object);
        for (const auto& [key, id] : arrayReferenceFieldArrays_)
            if (key.first == object)
            {
                arrayReferenceFlow_.escapedArrays.insert(id);
                arrayReferenceFlow_.unknownArrays.insert(id);
            }
    };
    std::vector<const ast::AstNode*> arguments(
        binding.selectedArgumentSyntaxes.begin(), binding.selectedArgumentSyntaxes.end());
    if (binding.syntax != nullptr && arguments.empty())
        for (const ast::AstNode* child : binding.syntax->children()) arguments.push_back(child);
    // EN: A call that receives two or more tracked objects (receiver included) may store any of them into
    //     a field of another: they are all exposed, and every one escapes as a receiver.
    // ES: Una llamada que recibe dos o mas objetos rastreados (receptor incluido) puede guardar cualquiera
    //     en un campo de otro: todos quedan expuestos y cada uno escapa como receptor.
    std::vector<const ast::AstNode*> reached = arguments;
    if (!binding.staticCall && binding.receiverSyntax != nullptr) reached.push_back(binding.receiverSyntax);
    ArrayReferenceFlowState::IdSet passed;
    for (const ast::AstNode* argument : reached)
        if (const auto local = localFor(argument))
            if (const auto* objects = objectsOf(*local)) passed.insert(objects->begin(), objects->end());
    for (const ast::AstNode* argument : arguments)
        if (const auto local = localFor(argument))
            if (const auto* objects = objectsOf(*local))
                for (const std::size_t object : *objects) escapeReceiver(object);
    if (passed.size() > 1) arrayReferenceFlow_.exposedObjects.insert(passed.begin(), passed.end());
    // EN: A store into an object that is not a local (a parameter, a field, a call result) is not
    //     followed: its values are exposed to unmodelled code.
    // ES: Un guardado en un objeto que no es un local (un parametro, un campo, un resultado de llamada) no
    //     se sigue: sus valores quedan expuestos a codigo no modelado.
    const auto exposeStores = [&](const std::vector<ArrayReferenceFieldEffect>& effects)
    {
        for (const ArrayReferenceFieldEffect& effect : effects)
        {
            if (effect.kind != ArrayReferenceFieldEffect::Kind::Store || !effect.sourceParameter.has_value()) continue;
            for (std::size_t i = 0; i < binding.selectedArgumentParameterOrdinals.size() &&
                 i < binding.selectedArgumentSyntaxes.size(); ++i)
                if (binding.selectedArgumentParameterOrdinals[i] == *effect.sourceParameter)
                    if (const auto source = localFor(binding.selectedArgumentSyntaxes[i]))
                        if (const auto* objects = objectsOf(*source))
                            arrayReferenceFlow_.exposedObjects.insert(objects->begin(), objects->end());
        }
    };
    const bool resolvedInstance = !binding.staticCall && binding.resolution == MethodCallResolutionKind::Resolved &&
        binding.selectedMemberOrdinal.has_value() && !binding.interfaceCall && target != nullptr &&
        !target->isVirtual && !target->isOverride && !target->isAbstract;
    if (binding.staticCall || binding.receiverSyntax == nullptr ||
        binding.receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        if (resolvedInstance) exposeStores(arrayReferenceReplaySource_->arrayReferenceFieldEffects(
            *binding.selectedMemberOrdinal));
        return;
    }
    const auto receiver = localFor(binding.receiverSyntax);
    if (!receiver.has_value())
    {
        if (resolvedInstance) exposeStores(arrayReferenceReplaySource_->arrayReferenceFieldEffects(
            *binding.selectedMemberOrdinal));
        return;
    }
    std::size_t object = kArrayReferenceUnknownReceiver;
    if (const auto* objects = objectsOf(*receiver);
        objects != nullptr && objects->size() == 1 && !arrayReferenceFlow_.maybeAbsentOwners.contains(*receiver))
        object = *objects->begin();
    const bool summarized = binding.resolution == MethodCallResolutionKind::Resolved &&
        binding.selectedMemberOrdinal.has_value() && !binding.interfaceCall && target != nullptr &&
        !target->isVirtual && !target->isOverride && !target->isAbstract;
    static const std::vector<ArrayReferenceFieldEffect> unknownAll{
        ArrayReferenceFieldEffect{ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false}};
    const std::vector<ArrayReferenceFieldEffect>& effects = summarized
        ? arrayReferenceReplaySource_->arrayReferenceFieldEffects(*binding.selectedMemberOrdinal) : unknownAll;
    if (effects.empty()) return;
    struct Use final
    {
        std::size_t array;
        std::optional<std::int64_t> index;
        bool definite;
    };
    std::vector<Use> uses;
    bool all = false;
    bool summarizedUse = false;
    for (const ArrayReferenceFieldEffect& effect : effects)
    {
        if (effect.kind == ArrayReferenceFieldEffect::Kind::UnknownAll) all = true;
        else if (effect.kind == ArrayReferenceFieldEffect::Kind::Unknown)
        {
            const std::size_t id = arrayReferenceFieldArray(object, effect.field);
            arrayReferenceFlow_.escapedArrays.insert(id);
            arrayReferenceFlow_.unknownArrays.insert(id);
        }
        else if (effect.kind == ArrayReferenceFieldEffect::Kind::Use)
        {
            uses.push_back({arrayReferenceFieldArray(object, effect.field), effect.index, effect.definite});
            summarizedUse = true;
        }
    }
    if (all)
    {
        if (object != kArrayReferenceUnknownReceiver) escapeReceiver(object);
        else markAllFieldsUnknown();
        // EN: The sentinel field stands for fields never stored through a summary; it is escaped exactly
        //     when the receiver escaped, so it contributes the exposed objects then.
        // ES: El campo centinela representa campos nunca guardados mediante un resumen; esta escapado
        //     exactamente cuando el receptor escapo, y entonces aporta los objetos expuestos.
        arrayReferenceFieldArray(object, kArrayReferenceUnknownReceiver);
        for (const auto& [key, id] : arrayReferenceFieldArrays_)
            if (key.first == object) uses.push_back({id, std::nullopt, false});
    }
    if (!uses.empty() && !arrayReferenceFlow_.stopped && !arrayReferenceNeverEntered_.contains(binding.syntax))
    {
        ArrayReferenceUseLife life = ArrayReferenceUseLife::NoLifeEvent;
        std::set<std::size_t> candidates;
        for (const Use& use : uses)
        {
            ArrayReferenceUseLife one = arrayReferenceFlow_.assessUse({use.array, use.index});
            if (one == ArrayReferenceUseLife::Dead && !use.definite) one = ArrayReferenceUseLife::Unproven;
            if (one == ArrayReferenceUseLife::Dead) life = one;
            else if (one == ArrayReferenceUseLife::Unproven && life == ArrayReferenceUseLife::NoLifeEvent) life = one;
            const auto ids = arrayReferenceFlow_.useCandidates({use.array, use.index});
            candidates.insert(ids.begin(), ids.end());
        }
        ArrayReferenceReplayObservation observation;
        observation.callableOrdinal = binding.callerMethodMemberOrdinal;
        observation.syntax = binding.syntax;
        observation.useLife = life;
        observation.fieldUse = true;
        observation.arrayRoot = uses.front().array;
        if (uses.size() == 1)
        {
            observation.arrayIndex = uses.front().index;
            observation.mustWritten = arrayReferenceFlow_.mustWritten({uses.front().array, uses.front().index});
        }
        observation.candidateObjects.assign(candidates.begin(), candidates.end());
        // EN: A call without a summarized use that names no tracked object cannot be rejected now or by
        //     the loop pass; a summarized use is kept for the loop pass (a later store in the loop).
        // ES: Una llamada sin uso resumido que no nombra ningun objeto rastreado no puede rechazarse ahora
        //     ni en la pasada de ciclos; un uso resumido se conserva para la pasada de ciclos (un guardado
        //     posterior en el ciclo).
        if (summarizedUse || life != ArrayReferenceUseLife::NoLifeEvent || !candidates.empty())
            arrayReferenceReplayObservations_.push_back(std::move(observation));
    }
    for (const ArrayReferenceFieldEffect& effect : effects)
    {
        if (effect.kind != ArrayReferenceFieldEffect::Kind::Store || !effect.sourceParameter.has_value()) continue;
        const ast::AstNode* argument = nullptr;
        for (std::size_t i = 0; i < binding.selectedArgumentParameterOrdinals.size() &&
             i < binding.selectedArgumentSyntaxes.size(); ++i)
            if (binding.selectedArgumentParameterOrdinals[i] == *effect.sourceParameter)
                argument = binding.selectedArgumentSyntaxes[i];
        const auto source = localFor(argument);
        const auto* objects = source.has_value() ? objectsOf(*source) : nullptr;
        if (objects == nullptr) continue;
        const ArrayReferenceFlowState::IdSet values = *objects;
        if (object == kArrayReferenceUnknownReceiver) markFieldUnknown(effect.field, values);
        const std::size_t id = arrayReferenceFieldArray(object, effect.field);
        if (effect.definite) arrayReferenceFlow_.write({id, effect.index}, values);
        else arrayReferenceFlow_.writeMaybe({id, effect.index}, values);
        if (object != kArrayReferenceUnknownReceiver)
            arrayReferenceFlow_.writeMaybe(
                {arrayReferenceFieldArray(kArrayReferenceUnknownReceiver, effect.field), std::nullopt}, values);
    }
}

// EN: Direct access `local.F` to a tracked reference field of a local receiver, from the caller. It may
//     read or replace what the methods stored, so it is a use at an unknown index and the receiver's
//     fields escape; every tracked object is exposed, because the access may store any of them.
// ES: Acceso directo `local.F` a un campo de referencia rastreado de un receptor local, desde el
//     llamador. Puede leer o reemplazar lo que guardaron los metodos, asi que es un uso con indice
//     desconocido y los campos del receptor escapan; todo objeto rastreado queda expuesto, porque el
//     acceso puede guardar cualquiera de ellos.
// Complexity: O(field arrays + owners). Effects: arrayReferenceFlow_, replay observations.
void ProjectMethodBodyModel::recordArrayReferenceFieldAccess(const MethodNameBinding& binding)
{
    if (binding.receiverSyntax == nullptr || binding.receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression ||
        binding.candidateMemberOrdinals.size() != 1 ||
        !arrayReferenceReplaySource_->arrayReferenceTrackedField(binding.candidateMemberOrdinals[0]).has_value())
        return;
    const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, binding.receiverSyntax->text());
    if (!name.has_value() || name->kind != ScopeNameKind::Local) return;
    std::size_t object = kArrayReferenceUnknownReceiver;
    if (const auto owner = arrayReferenceFlow_.owners.find(name->semanticOrdinal);
        owner != arrayReferenceFlow_.owners.end() && owner->second.size() == 1 &&
        !arrayReferenceFlow_.maybeAbsentOwners.contains(name->semanticOrdinal))
        object = *owner->second.begin();
    const std::size_t id = arrayReferenceFieldArray(object, binding.candidateMemberOrdinals[0]);
    if (!arrayReferenceFlow_.stopped && !arrayReferenceNeverEntered_.contains(binding.syntax))
    {
        ArrayReferenceReplayObservation observation;
        observation.callableOrdinal = binding.methodMemberOrdinal;
        observation.syntax = binding.syntax;
        observation.useLife = arrayReferenceFlow_.assessUse({id, std::nullopt});
        if (observation.useLife == ArrayReferenceUseLife::Dead) observation.useLife = ArrayReferenceUseLife::Unproven;
        observation.fieldUse = true;
        observation.arrayRoot = id;
        const auto candidates = arrayReferenceFlow_.useCandidates({id, std::nullopt});
        observation.candidateObjects.assign(candidates.begin(), candidates.end());
        if (observation.useLife != ArrayReferenceUseLife::NoLifeEvent || !candidates.empty())
            arrayReferenceReplayObservations_.push_back(std::move(observation));
    }
    arrayReferenceFieldEscapedReceivers_.insert(object);
    for (const auto& [key, field] : arrayReferenceFieldArrays_)
        if (key.first == object)
        {
            arrayReferenceFlow_.escapedArrays.insert(field);
            arrayReferenceFlow_.unknownArrays.insert(field);
        }
    for (const auto& [owner, ids] : arrayReferenceFlow_.owners)
        arrayReferenceFlow_.exposedObjects.insert(ids.begin(), ids.end());
}
}