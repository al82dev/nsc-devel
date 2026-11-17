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
// EN: The receiver of the replayed instance callable (`this`), used as a receiver object of its own.
// ES: El receptor del callable de instancia reproducido (`this`), usado como objeto receptor propio.
constexpr std::size_t kArrayReferenceThisReceiver = std::numeric_limits<std::size_t>::max() - 1;
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
bool ProjectMethodBodyModel::precompiledReceiverHasNoReferenceFieldEffects(std::size_t callable) const
{
    if (!arrayReferenceFieldEffects(callable).empty()) return false;
    for (const MethodCallBinding& call : calls())
        if (call.callerMethodMemberOrdinal == callable && !call.staticCall) return false;
    return true;
}

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
    if (symbol == nullptr || memberModel_ == nullptr || symbol->kind != ClassMemberKind::Method) return none;
    if (symbol->isPrecompiled || symbol->syntax == nullptr || symbol->syntax->first() == nullptr)
        return arrayReferenceFieldEffects_.emplace(callable, unknownAll).first->second;
    arrayReferenceFieldEffectsInProgress_.insert(callable);
    std::vector<ArrayReferenceFieldEffect> effects;
    const ast::AstNode* body = symbol->syntax->first();
    const std::size_t memberCount = memberModel_->members().size();
    const auto fieldOf = [&](const ast::AstNode* node) -> std::optional<std::pair<std::size_t, bool>>
    {
        if (node->kind() != ast::AstNodeKind::MemberExpression &&
            node->kind() != ast::AstNodeKind::IdentifierExpression) return std::nullopt;
        const MethodNameBinding* binding = nameBinding(node, callable);
        if (binding == nullptr || (binding->kind != MethodNameBindingKind::ImplicitField &&
                                   binding->kind != MethodNameBindingKind::ExplicitThisField &&
                                   binding->kind != MethodNameBindingKind::StaticField) ||
            binding->candidateMemberOrdinals.size() != 1 || binding->candidateMemberOrdinals[0] >= memberCount)
            return std::nullopt;
        if (node->kind() == ast::AstNodeKind::MemberExpression && binding->kind != MethodNameBindingKind::StaticField &&
            (node->first() == nullptr || node->first()->kind() != ast::AstNodeKind::ThisExpression))
            return std::nullopt;
        const std::size_t ordinal = binding->candidateMemberOrdinals[0];
        const auto tracked = arrayReferenceTrackedField(ordinal, true);
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
    // EN: A class-reference parameter: storing or passing it anywhere that is not modelled lets it reach
    //     code the summary does not describe.
    // ES: Un parametro de referencia de clase: guardarlo o pasarlo donde no se modela lo deja llegar a
    //     codigo que el resumen no describe.
    const auto referenceParameterOf = [&](const ast::AstNode* node) -> std::optional<std::size_t>
    {
        if (node != nullptr && node->kind() == ast::AstNodeKind::MoveExpression) node = node->first();
        const auto parameter = parameterOf(node);
        if (!parameter.has_value() || *parameter >= symbol->parameters.size()) return std::nullopt;
        const std::string& type = symbol->parameters[*parameter].typeKey;
        return type != "builtin:string" && (isReferenceTypeKey(type) || ProjectTypeConversion::isNativeArray(type))
            ? parameter : std::nullopt;
    };
    const auto pushUnknownAll = [&]
    {
        effects.push_back({ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false});
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
            pushUnknownAll();
            continue;
        }
        if (node->kind() == ast::AstNodeKind::ThisExpression)
        {
            if (parent == nullptr || parent->kind() != ast::AstNodeKind::MemberExpression || parent->first() != node)
                pushUnknownAll();
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
                    {
                        if (const auto parameter = parameterOf(parent->second()))
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Store, field->first, parameter, 0,
                                definiteAt(std::vector<const ast::AstNode*>(
                                    frame.ancestors.begin(), frame.ancestors.end() - 1))});
                        else if (parent->second() != nullptr &&
                                 parent->second()->kind() != ast::AstNodeKind::NullLiteralExpression)
                            // EN: Any other stored value (an element or field of a parameter, a call result) is
                            //     not followed: the field becomes unknown (doc 15).
                            // ES: Cualquier otro valor guardado (elemento o campo de un parametro, resultado de
                            //     una llamada) no se sigue: el campo queda desconocido (doc 15).
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Unknown, field->first, std::nullopt,
                                std::nullopt, false});
                    }
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
                    {
                        if (const auto parameter = parameterOf(grand->second()))
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Store, field->first, parameter, index,
                                definiteAt(std::vector<const ast::AstNode*>(
                                    frame.ancestors.begin(), frame.ancestors.end() - 1))});
                        else if (grand->second() != nullptr &&
                                 grand->second()->kind() != ast::AstNodeKind::NullLiteralExpression)
                            // EN: Any other stored value (an element or field of a parameter, a call result) is
                            //     not followed: the field becomes unknown (doc 15).
                            // ES: Cualquier otro valor guardado (elemento o campo de un parametro, resultado de
                            //     una llamada) no se sigue: el campo queda desconocido (doc 15).
                            effects.push_back({ArrayReferenceFieldEffect::Kind::Unknown, field->first, std::nullopt,
                                std::nullopt, false});
                    }
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
        // EN: `expr.F` on a receiver other than `this` (another object, possibly reached through an owned
        //     field): a use of the unknown receiver's field F (comparisons and assignment targets excepted).
        // ES: `expr.F` sobre un receptor distinto de `this` (otro objeto, quizas alcanzado por un campo owned):
        //     un uso del campo F del receptor desconocido (salvo comparaciones y destinos de asignacion).
        if (node->kind() == ast::AstNodeKind::MemberExpression)
            if (const MethodNameBinding* binding = nameBinding(node, callable);
                binding != nullptr && binding->kind == MethodNameBindingKind::ExplicitObjectField &&
                binding->candidateMemberOrdinals.size() == 1 &&
                arrayReferenceTrackedField(binding->candidateMemberOrdinals[0]).has_value() && !isComparison(parent) &&
                !(parent != nullptr && parent->kind() == ast::AstNodeKind::AssignmentExpression &&
                  parent->first() == node))
                effects.push_back({ArrayReferenceFieldEffect::Kind::Use, binding->candidateMemberOrdinals[0],
                    std::nullopt, std::nullopt, false, true});
        // EN: `target = p` where the target is not a modelled field or field slot, and `T x = p`, store the
        //     parameter somewhere the summary does not follow.
        // ES: `destino = p` donde el destino no es un campo o ranura de campo modelado, y `T x = p`, guardan
        //     el parametro donde el resumen no lo sigue.
        if (node->kind() == ast::AstNodeKind::AssignmentExpression && referenceParameterOf(node->second()))
        {
            const ast::AstNode* target = node->first();
            // EN: `arrayParameter[i] = p` is modelled by the array write summaries of the caller's replay.
            // ES: `parametroArreglo[i] = p` lo modelan los resumenes de escritura de arreglos del replay.
            const bool modelled = target != nullptr && node->tokenKind() == lexer::TokenKind::Equal &&
                (fieldOf(target).has_value() ||
                 (target->kind() == ast::AstNodeKind::IndexExpression && target->first() != nullptr &&
                  (fieldOf(target->first()).has_value() || parameterOf(target->first()).has_value())));
            if (!modelled) pushUnknownAll();
        }
        if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement && referenceParameterOf(node->first()))
            pushUnknownAll();
        if (node->kind() == ast::AstNodeKind::ObjectCreationExpression)
            for (const ast::AstNode* argument : node->children())
                if (referenceParameterOf(argument)) pushUnknownAll();
        if (node->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCallBinding* call = callBinding(node, callable);
            const bool thisCall = call != nullptr && !call->staticCall &&
                (call->receiverSyntax == nullptr || call->receiverSyntax->kind() == ast::AstNodeKind::ThisExpression);
            if (!thisCall)
            {
                std::vector<const ast::AstNode*> arguments;
                if (call != nullptr) arguments = call->selectedArgumentSyntaxes;
                else for (const ast::AstNode* child : node->children()) arguments.push_back(child);
                for (const ast::AstNode* argument : arguments)
                    if (referenceParameterOf(argument)) pushUnknownAll();
            }
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
//     for a native array of class references, false for a plain class reference. `owned` fields are
//     excluded (they follow the ownership rules); static fields only when `allowStatic` (they are always
//     read and written through the unknown receiver, doc 08).
// ES: Si un campo guarda referencias de clase que siguen las rutas de campo: nullopt si no, true para un
//     arreglo nativo de referencias de clase, false para una referencia de clase simple. Se excluyen los
//     campos `owned` (siguen las reglas de ownership); los static solo con `allowStatic` (siempre se
//     leen y escriben por el receptor desconocido, doc 08).
// Complexity: O(1). Effects: none.
std::optional<bool> ProjectMethodBodyModel::arrayReferenceTrackedField(
    std::size_t fieldOrdinal, bool allowStatic) const
{
    if (memberModel_ == nullptr || fieldOrdinal >= memberModel_->members().size()) return std::nullopt;
    const ClassMemberSymbol& field = memberModel_->members()[fieldOrdinal];
    // EN: An owned plain field holds an object it owns (adoption, doc 04); an owned array field may still
    //     receive elements moved out of a borrowed array (`items[i] = move(values[i])`), so it is tracked.
    // ES: Un campo owned simple guarda un objeto que posee (adopcion, doc 04); un arreglo owned en campo
    //     puede recibir elementos movidos desde un arreglo prestado (`items[i] = move(values[i])`), asi que
    //     se rastrea.
    if (field.kind != ClassMemberKind::Field || (field.isStatic && !allowStatic) ||
        (field.declaredOwnership == sema::OwnershipKind::Owned &&
         !ProjectTypeConversion::isNativeArray(field.declaredTypeKey))) return std::nullopt;
    // EN: A generic parameter type (`T`, `T[]`) may be instantiated with a class: it is tracked (doc 15).
    // ES: Un tipo parametro generico (`T`, `T[]`) puede instanciarse con una clase: se rastrea (doc 15).
    const auto mayReference = [&](const std::string& type)
    {
        if (type == "builtin:string") return false;
        if (isReferenceTypeKey(type)) return true;
        return type.rfind("builtin:", 0) != 0 && !ProjectTypeConversion::isNativeArray(type) &&
            !namedTypeOrdinalForTypeKey(type).has_value();
    };
    if (ProjectTypeConversion::isNativeArray(field.declaredTypeKey))
    {
        const auto element = ProjectTypeConversion::nativeArrayElementType(field.declaredTypeKey);
        if (!element.has_value() || !mayReference(*element)) return std::nullopt;
        return true;
    }
    if (!mayReference(field.declaredTypeKey)) return std::nullopt;
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
    const auto markFieldUnknown = [&](std::size_t field, const ArrayReferenceFlowState::IdSet& values)
    {
        arrayReferenceFieldUnknownFields_.insert(field);
        arrayReferenceFlow_.routeExposed.insert(values.begin(), values.end());
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
    // EN: Exposure at the call boundary (doc 08). A tracked object that reaches a call together with
    //     anything else that can hold references (the receiver, including an implicit `this`, or another
    //     argument of class, struct or array type that is not itself a tracked owner) may be stored there:
    //     it is exposed. Every tracked object passed as an argument escapes as a receiver.
    // ES: Exposicion en la frontera de la llamada (doc 08). Un objeto rastreado que llega a una llamada
    //     junto con algo mas que puede guardar referencias (el receptor, incluido un `this` implicito, u
    //     otro argumento de tipo clase, struct o arreglo que no es un dueno rastreado) puede guardarse
    //     alli: queda expuesto. Todo objeto rastreado pasado como argumento escapa como receptor.
    const auto mayHoldReferences = [&](const ast::AstNode* expression)
    {
        if (expression == nullptr) return false;
        if (expression->kind() == ast::AstNodeKind::MoveExpression ||
            (expression->kind() == ast::AstNodeKind::UnaryExpression &&
             expression->tokenKind() == lexer::TokenKind::Amp)) expression = expression->first();
        if (expression == nullptr) return false;
        if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
            expression->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
            expression->kind() == ast::AstNodeKind::StringLiteralExpression) return false;
        const std::string_view type = recordedExpressionTypeKey(expression, binding.callerMethodMemberOrdinal);
        return type.empty() || (type != "builtin:string" &&
            (type.rfind("builtin:", 0) != 0 || ProjectTypeConversion::isNativeArray(type)));
    };
    // EN: A `ref`/`out`/`in` argument naming a tracked array or one of its elements lets the callee write
    //     the slot or replace the array: the array escapes and becomes unknown.
    // ES: Un argumento `ref`/`out`/`in` que nombra un arreglo rastreado o uno de sus elementos deja al
    //     callee escribir la ranura o reemplazar el arreglo: el arreglo escapa y queda desconocido.
    for (const ast::AstNode* argument : arguments)
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::UnaryExpression ||
            argument->tokenKind() != lexer::TokenKind::Amp || argument->first() == nullptr) continue;
        const ast::AstNode* operand = argument->first();
        if (operand->kind() == ast::AstNodeKind::IndexExpression) operand = operand->first();
        if (const auto local = localFor(operand))
            if (const auto root = arrayReferenceFlow_.arrayForLocal(*local))
            {
                arrayReferenceFlow_.escapedArrays.insert(*root);
                arrayReferenceFlow_.unknownArrays.insert(*root);
            }
    }
    // EN: An argument whose parameter the analyzed, non-virtual callee body only reads (a plain field read
    //     of it, or an `==`/`!=` operand) is not stored anywhere by the call: it is not route-exposed.
    //     Any other occurrence (a call on it, passing it on, assigning it, taking a reference), a lambda,
    //     inline assembly or an unsafe block keeps the conservative exposure. The parameter escape summary
    //     is not used: it does not count a store into an array element. See
    //     docs/fixes/argumento-solo-leido-no-expuesto.md.
    // ES: Un argumento cuyo parametro el cuerpo analizado y no virtual del callee solo lee (una lectura
    //     simple de un campo suyo, o un operando de `==`/`!=`) no se guarda en ninguna parte en la llamada:
    //     no queda expuesto por ruta. Cualquier otra aparicion (una llamada sobre el, pasarlo, asignarlo,
    //     tomar una referencia), una lambda, ensamblador en linea o un bloque unsafe conservan la
    //     exposicion conservadora. No se usa el resumen de escape de parametros: no cuenta un guardado en
    //     un elemento de arreglo. Ver docs/fixes/argumento-solo-leido-no-expuesto.md.
    const auto provenNotStored = [&](std::size_t index)
    {
        const ProjectMethodBodyModel* source = arrayReferenceReplaySource_;
        if (binding.resolution != MethodCallResolutionKind::Resolved || !binding.selectedMemberOrdinal.has_value() ||
            binding.interfaceCall || target == nullptr || target->isVirtual || target->isOverride ||
            target->isAbstract || target->isPrecompiled || target->isUnsafe || target->isReferenceReturn ||
            target->effects.contains(sema::Effect::Async) || target->effects.contains(sema::Effect::Generator) ||
            target->syntax == nullptr || target->syntax->first() == nullptr ||
            index >= binding.selectedArgumentSyntaxes.size() ||
            index >= binding.selectedArgumentParameterOrdinals.size()) return false;
        const std::size_t callee = *binding.selectedMemberOrdinal;
        const std::size_t parameter = binding.selectedArgumentParameterOrdinals[index];
        if (parameter >= target->parameters.size() ||
            target->parameters[parameter].passKind != ParameterPassKind::Value ||
            !source->callableBodyAnalyzed(callee)) return false;
        struct Visit final { const ast::AstNode* node; const ast::AstNode* parent; const ast::AstNode* grand; };
        std::vector<Visit> pending{{target->syntax->first(), nullptr, nullptr}};
        while (!pending.empty())
        {
            const Visit visit = pending.back();
            pending.pop_back();
            const ast::AstNode* node = visit.node;
            if (node == nullptr) continue;
            if (node->kind() == ast::AstNodeKind::LambdaExpression ||
                node->kind() == ast::AstNodeKind::InlineAssemblyStatement || node->auxiliaryText() == "unsafe-block")
                return false;
            const MethodNameBinding* name = source->nameBinding(node, callee);
            if (name != nullptr && name->kind == MethodNameBindingKind::Parameter &&
                name->parameterOrdinal == parameter)
            {
                const ast::AstNode* parent = visit.parent;
                const bool comparison = parent != nullptr && parent->kind() == ast::AstNodeKind::BinaryExpression &&
                    (parent->tokenKind() == lexer::TokenKind::EqualEqual ||
                     parent->tokenKind() == lexer::TokenKind::BangEqual);
                bool fieldRead = false;
                if (parent != nullptr && parent->kind() == ast::AstNodeKind::MemberExpression &&
                    parent->first() == node && !parent->isReference() && visit.grand != nullptr &&
                    !visit.grand->isReference() &&
                    !(visit.grand->kind() == ast::AstNodeKind::CallExpression && visit.grand->first() == parent) &&
                    !(visit.grand->kind() == ast::AstNodeKind::UnaryExpression &&
                      visit.grand->tokenKind() == lexer::TokenKind::Amp))
                {
                    const MethodNameBinding* field = source->nameBinding(parent, callee);
                    fieldRead = field != nullptr && field->kind == MethodNameBindingKind::ExplicitObjectField &&
                        field->candidateMemberOrdinals.size() == 1U && memberModel_ != nullptr &&
                        field->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                        memberModel_->members()[field->candidateMemberOrdinals.front()].kind ==
                            ClassMemberKind::Field;
                }
                if (!comparison && !fieldRead) return false;
            }
            pending.push_back({node->first(), node, visit.parent});
            pending.push_back({node->second(), node, visit.parent});
            pending.push_back({node->third(), node, visit.parent});
            for (const ast::AstNode* child : node->children()) pending.push_back({child, node, visit.parent});
        }
        return true;
    };
    ArrayReferenceFlowState::IdSet passed;
    bool container = !binding.staticCall;
    for (std::size_t argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
    {
        const ast::AstNode* argument = arguments[argumentIndex];
        if (argument != nullptr && argument->kind() == ast::AstNodeKind::ThisExpression)
        {
            escapeReceiver(kArrayReferenceThisReceiver);
            container = true;
            continue;
        }
        const auto local = localFor(argument);
        const auto* objects = local.has_value() ? objectsOf(*local) : nullptr;
        if (objects != nullptr)
        {
            // EN: A moved argument transfers ownership; it is not a borrowed reference left behind.
            // ES: Un argumento movido transfiere ownership; no es una referencia prestada que queda atras.
            if (argument->kind() != ast::AstNodeKind::MoveExpression && !provenNotStored(argumentIndex))
                passed.insert(objects->begin(), objects->end());
            for (const std::size_t object : *objects) escapeReceiver(object);
        }
        // EN: A non-owning local passes what it may name, like a borrowed owner (doc 20).
        // ES: Un local no dueno pasa lo que puede nombrar, como un dueno prestado (doc 20).
        else if (const auto recorded = local.has_value()
                     ? arrayReferenceValueLocals_.find(*local) : arrayReferenceValueLocals_.end();
                 recorded != arrayReferenceValueLocals_.end())
        {
            if (!provenNotStored(argumentIndex))
                passed.insert(recorded->second.objects.begin(), recorded->second.objects.end());
        }
        else if (const auto root = local.has_value() ? arrayReferenceFlow_.arrayForLocal(*local) : std::nullopt)
        {
            // EN: A tracked local array: the array write summaries already model what the call stores in it,
            //     but the callee may copy its elements anywhere (a library AddRange, a field store of `a[i]`):
            //     every object it holds is exposed (route R03b, doc 15).
            // ES: Un arreglo local rastreado: los resumenes de escritura ya modelan lo que la llamada guarda en
            //     el, pero el callee puede copiar sus elementos a cualquier parte (un AddRange de biblioteca, un
            //     guardado de `a[i]` en un campo): todo objeto que guarda queda expuesto (ruta R03b, doc 15).
            for (const auto& [slot, ids] : arrayReferenceFlow_.slots)
                if (slot.array == *root) arrayReferenceFlow_.routeExposed.insert(ids.begin(), ids.end());
        }
        else if (mayHoldReferences(argument) ||
                 (argument != nullptr && argument->kind() == ast::AstNodeKind::UnaryExpression &&
                  argument->tokenKind() == lexer::TokenKind::Amp)) container = true;
    }
    // EN: The receiver's own objects are exposed only when the callee may publish `this` (UnknownAll below).
    // ES: Los objetos del propio receptor solo se exponen cuando el callee puede publicar `this` (UnknownAll).
    ArrayReferenceFlowState::IdSet receiverObjects;
    if (!binding.staticCall && binding.receiverSyntax != nullptr)
        if (const auto local = localFor(binding.receiverSyntax))
            if (const auto* objects = objectsOf(*local)) receiverObjects = *objects;
    if (container || passed.size() > 1) arrayReferenceFlow_.routeExposed.insert(passed.begin(), passed.end());
    // EN: The receiver object: a single tracked owner, or the unknown receiver for anything else
    //     (aliases, parameters, member chains, call results, an implicit `this`). Static fields always use
    //     the unknown receiver.
    // ES: El objeto receptor: un unico dueno rastreado, o el receptor desconocido para todo lo demas
    //     (alias, parametros, cadenas de miembros, resultados de llamadas, un `this` implicito). Los campos
    //     static siempre usan el receptor desconocido.
    std::size_t object = kArrayReferenceUnknownReceiver;
    if (!binding.staticCall && (binding.receiverSyntax == nullptr ||
                                binding.receiverSyntax->kind() == ast::AstNodeKind::ThisExpression))
        object = kArrayReferenceThisReceiver;
    if (!binding.staticCall && binding.receiverSyntax != nullptr)
        if (const auto receiver = localFor(binding.receiverSyntax))
            if (const auto* objects = objectsOf(*receiver);
                objects != nullptr && objects->size() == 1 &&
                !arrayReferenceFlow_.maybeAbsentOwners.contains(*receiver))
                object = *objects->begin();
    const bool summarized = binding.resolution == MethodCallResolutionKind::Resolved &&
        binding.selectedMemberOrdinal.has_value() && !binding.interfaceCall && target != nullptr &&
        !target->isVirtual && !target->isOverride && !target->isAbstract;
    static const std::vector<ArrayReferenceFieldEffect> unknownAll{
        ArrayReferenceFieldEffect{ArrayReferenceFieldEffect::Kind::UnknownAll, 0, std::nullopt, std::nullopt, false}};
    static const std::vector<ArrayReferenceFieldEffect> noEffects;
    // The producer proved this non-virtual precompiled method has neither a
    // receiver escape nor a tracked reference-field effect. Its empty summary
    // must not be replaced by synthetic uses of every receiver field.
    const bool provenNoEffects = summarized && target->isPrecompiled && target->receiverNoReferenceFieldEffects;
    const std::vector<ArrayReferenceFieldEffect>& effects = provenNoEffects ? noEffects : summarized
        ? arrayReferenceReplaySource_->arrayReferenceFieldEffects(*binding.selectedMemberOrdinal) : unknownAll;
    if (effects.empty()) return;
    const auto objectFor = [&](const ArrayReferenceFieldEffect& effect)
    {
        return effect.foreignReceiver || (memberModel_ != nullptr && effect.field < memberModel_->members().size() &&
            memberModel_->members()[effect.field].isStatic) ? kArrayReferenceUnknownReceiver : object;
    };
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
            const std::size_t id = arrayReferenceFieldArray(objectFor(effect), effect.field);
            arrayReferenceFlow_.escapedArrays.insert(id);
            arrayReferenceFlow_.unknownArrays.insert(id);
        }
        else if (effect.kind == ArrayReferenceFieldEffect::Kind::Use)
        {
            uses.push_back({arrayReferenceFieldArray(objectFor(effect), effect.field), effect.index, effect.definite});
            summarizedUse = true;
        }
    }
    // EN: The call's own exposure (what unmodelled code may store during this call) is applied after its
    //     uses are assessed: the uses name what the receiver already stored before the call, so a fresh
    //     receiver's first unknown call is not a use of objects exposed elsewhere before it escaped. See
    //     docs/fixes/uso-antes-del-escape-de-la-llamada.md.
    // ES: La exposicion propia de la llamada (lo que codigo no modelado puede guardar durante ella) se
    //     aplica despues de evaluar sus usos: los usos nombran lo que el receptor ya guardaba antes de la
    //     llamada, asi la primera llamada desconocida de un receptor nuevo no usa objetos expuestos en otra
    //     parte antes de que escapara. Ver docs/fixes/uso-antes-del-escape-de-la-llamada.md.
    const auto applyUnknownCallExposure = [&]()
    {
        if (!all) return;
        // EN: Unmodelled code may store any tracked object the call received anywhere.
        // ES: Codigo no modelado puede guardar en cualquier parte todo objeto rastreado que recibio la llamada.
        arrayReferenceFlow_.routeExposed.insert(passed.begin(), passed.end());
        // EN: Code reached through an unknown receiver can only store what it received (exposed above) and
        //     can only reach receivers that already escaped.
        // ES: El codigo alcanzado por un receptor desconocido solo puede guardar lo que recibio (expuesto
        //     arriba) y solo alcanza receptores que ya escaparon.
        if (object != kArrayReferenceUnknownReceiver)
        {
            if (!(summarized && target->isPrecompiled && target->receiverNoReferenceFieldEffects))
            {
                escapeReceiver(object);
                arrayReferenceFlow_.routeExposed.insert(receiverObjects.begin(), receiverObjects.end());
            }
        }
    };
    if (all)
    {
        // EN: The sentinel field stands for fields never stored through a summary; it is escaped exactly
        //     when the receiver escaped, so it contributes the exposed objects then.
        // ES: El campo centinela representa campos nunca guardados mediante un resumen; esta escapado
        //     exactamente cuando el receptor escapo, y entonces aporta los objetos expuestos.
        // EN: Only a tracked local receiver is used: an unknown call on any other receiver is not a use of
        //     what other objects stored (the rule applies to uses, not to unknown code).
        // ES: Solo se usa un receptor local rastreado: una llamada desconocida sobre otro receptor no es un
        //     uso de lo que otros objetos guardaron (la regla aplica a usos, no a codigo desconocido).
        if (object != kArrayReferenceUnknownReceiver)
        {
            arrayReferenceFieldArray(object, kArrayReferenceUnknownReceiver);
            for (const auto& [key, id] : arrayReferenceFieldArrays_)
                if (key.first == object) uses.push_back({id, std::nullopt, false});
        }
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
        if (life != ArrayReferenceUseLife::NoLifeEvent || (object != kArrayReferenceUnknownReceiver && summarizedUse))
            arrayReferenceReplayObservations_.push_back(std::move(observation));
    }
    applyUnknownCallExposure();
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
        const std::size_t receiverObject = objectFor(effect);
        if (receiverObject == kArrayReferenceUnknownReceiver) markFieldUnknown(effect.field, values);
        const std::size_t id = arrayReferenceFieldArray(receiverObject, effect.field);
        if (effect.definite) arrayReferenceFlow_.write({id, effect.index}, values);
        else arrayReferenceFlow_.writeMaybe({id, effect.index}, values);
        if (receiverObject != kArrayReferenceUnknownReceiver)
            arrayReferenceFlow_.writeMaybe(
                {arrayReferenceFieldArray(kArrayReferenceUnknownReceiver, effect.field), std::nullopt}, values);
    }
}

// EN: Field reads in the replayed callable (doc 08). `local.F` on a tracked local receiver is a use of that
//     receiver's field at an unknown index; `F`, `this.F`, `S.F` (static) and `expr.F` on any other
//     receiver are uses of the unknown receiver's field. A direct access cannot store a borrowed reference
//     to a tracked owner: the language rejects copying a `raii` owner (`local.F = owner`), and
//     `local.F = move(owner)` is an ownership transfer handled by the move rules; borrowed stores only
//     happen through calls, which the call boundary exposes.
// ES: Lecturas de campo en el callable reproducido (doc 08). `local.F` sobre un receptor local rastreado es
//     un uso del campo de ese receptor con indice desconocido; `F`, `this.F`, `S.F` (static) y `expr.F`
//     sobre cualquier otro receptor son usos del campo del receptor desconocido. Un acceso directo no
//     puede guardar una referencia prestada a un dueno rastreado: el lenguaje rechaza copiar un dueno
//     `raii` (`local.F = dueno`), y `local.F = move(dueno)` es una transferencia de ownership que tratan
//     las reglas de move; los guardados prestados solo ocurren por llamadas, que expone la frontera de la
//     llamada.
// Complexity: O(field arrays + owners). Effects: arrayReferenceFlow_, replay observations.
void ProjectMethodBodyModel::recordArrayReferenceFieldAccess(const MethodNameBinding& binding)
{
    if (binding.candidateMemberOrdinals.size() != 1) return;
    const auto tracked =
        arrayReferenceReplaySource_->arrayReferenceTrackedField(binding.candidateMemberOrdinals[0], true);
    if (!tracked.has_value()) return;
    std::optional<std::size_t> local;
    if (binding.kind == MethodNameBindingKind::ExplicitObjectField && binding.receiverSyntax != nullptr &&
        binding.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
        if (const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, binding.receiverSyntax->text());
            name.has_value() && name->kind == ScopeNameKind::Local)
            local = name->semanticOrdinal;
    std::size_t object = binding.kind == MethodNameBindingKind::ImplicitField ||
        binding.kind == MethodNameBindingKind::ExplicitThisField ? kArrayReferenceThisReceiver
                                                                 : kArrayReferenceUnknownReceiver;
    if (local.has_value())
        if (const auto owner = arrayReferenceFlow_.owners.find(*local);
            owner != arrayReferenceFlow_.owners.end() && owner->second.size() == 1 &&
            !arrayReferenceFlow_.maybeAbsentOwners.contains(*local))
            object = *owner->second.begin();
    const std::size_t id = arrayReferenceFieldArray(object, binding.candidateMemberOrdinals[0]);
    // EN: Element reads of an own or static array field are assessed by the index read itself.
    // ES: Las lecturas de elementos de un arreglo en campo propio o static las evalua la lectura indexada.
    if (!arrayReferenceFlow_.stopped && !arrayReferenceNeverEntered_.contains(binding.syntax) &&
        (local.has_value() || !*tracked))
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
        if (observation.useLife != ArrayReferenceUseLife::NoLifeEvent ||
            (object != kArrayReferenceUnknownReceiver && !candidates.empty()))
            arrayReferenceReplayObservations_.push_back(std::move(observation));
    }
}

// EN: A user indexer read (`receiver[i]`, a synthetic `get_Item` call) is a use of every reference the
//     receiver may hold: the field pseudo arrays of a tracked local receiver (and its sentinel, escaped when
//     the receiver escaped), or the unknown receiver otherwise (route R03b, doc 15). A destroyed object is
//     never reported as proven here: the getter's path is not modelled.
// ES: Una lectura de indexador de usuario (`receptor[i]`, una llamada sintetica `get_Item`) es un uso de
//     toda referencia que el receptor puede guardar: los pseudoarreglos de campo de un receptor local
//     rastreado (y su centinela, escapado si el receptor escapo), o el receptor desconocido en otro caso
//     (ruta R03b, doc 15). Aqui nunca se reporta un objeto destruido como demostrado: el camino del getter
//     no se modela.
// Complexity: O(field arrays * candidates). Effects: arrayReferenceFieldArrays_, replay observations.
void ProjectMethodBodyModel::recordArrayReferenceIndexerUse(const ast::AstNode* expression,
    const ast::AstNode* receiver, std::size_t scopeOrdinal, std::size_t callable)
{
    if (arrayReferenceFlow_.stopped || arrayReferenceNeverEntered_.contains(expression)) return;
    std::size_t object = kArrayReferenceUnknownReceiver;
    if (receiver != nullptr && receiver->kind() == ast::AstNodeKind::IdentifierExpression)
        if (const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, receiver->text());
            name.has_value() && name->kind == ScopeNameKind::Local)
            if (const auto owner = arrayReferenceFlow_.owners.find(name->semanticOrdinal);
                owner != arrayReferenceFlow_.owners.end() && owner->second.size() == 1 &&
                !arrayReferenceFlow_.maybeAbsentOwners.contains(name->semanticOrdinal))
                object = *owner->second.begin();
    std::vector<std::size_t> arrays;
    if (object == kArrayReferenceUnknownReceiver)
        arrays.push_back(arrayReferenceFieldArray(kArrayReferenceUnknownReceiver, kArrayReferenceUnknownReceiver));
    else
    {
        arrayReferenceFieldArray(object, kArrayReferenceUnknownReceiver);
        for (const auto& [key, id] : arrayReferenceFieldArrays_)
            if (key.first == object) arrays.push_back(id);
    }
    ArrayReferenceUseLife life = ArrayReferenceUseLife::NoLifeEvent;
    std::set<std::size_t> candidates;
    for (const std::size_t array : arrays)
    {
        const ArrayReferenceUseLife one = arrayReferenceFlow_.assessUse({array, std::nullopt});
        if (one != ArrayReferenceUseLife::NoLifeEvent) life = ArrayReferenceUseLife::Unproven;
        const auto ids = arrayReferenceFlow_.useCandidates({array, std::nullopt});
        candidates.insert(ids.begin(), ids.end());
    }
    if (life == ArrayReferenceUseLife::NoLifeEvent && (object == kArrayReferenceUnknownReceiver || candidates.empty()))
        return;
    ArrayReferenceReplayObservation observation;
    observation.callableOrdinal = callable;
    observation.syntax = expression;
    observation.useLife = life;
    observation.fieldUse = true;
    observation.arrayRoot = arrays.front();
    observation.candidateObjects.assign(candidates.begin(), candidates.end());
    arrayReferenceReplayObservations_.push_back(std::move(observation));
}

// EN: Element reads whose array is not a tracked local (a parameter, a field, a nested index, a call
//     result): the array may hold any exposed object, so the read is assessed against the unknown
//     receiver's sentinel pseudo array (escaped, candidates = exposed objects) (doc 08).
// ES: Lecturas de elemento cuyo arreglo no es un local rastreado (un parametro, un campo, un indice
//     anidado, un resultado de llamada): el arreglo puede guardar cualquier objeto expuesto, asi que la
//     lectura se evalua contra el pseudoarreglo centinela del receptor desconocido (escapado, candidatos =
//     objetos expuestos) (doc 08).
// Complexity: O(exposed objects). Effects: arrayReferenceFieldArrays_.
ArrayReferenceUseLife ProjectMethodBodyModel::assessUntrackedArrayReferenceRead()
{
    const ArrayReferenceUseLife life = arrayReferenceFlow_.assessUse(
        {arrayReferenceFieldArray(kArrayReferenceUnknownReceiver, kArrayReferenceUnknownReceiver), std::nullopt});
    return life == ArrayReferenceUseLife::Dead ? ArrayReferenceUseLife::Unproven : life;
}

// EN: Tracked objects that the reference value `value` may name when the replay copies it into an
//     element or a local (doc 20). Literals, `null`, a new object, `this`, a parameter and a move (whose
//     ownership effect is followed elsewhere) name no tracked object. A tracked owner names its objects
//     and a recorded non-owning local names what was recorded for it; both are exact (the value is one of
//     those objects). An element read of a tracked local array names that element's candidates, but the
//     element may also hold an untracked object, so the answer is not exact. Anything else (a call
//     result, a member, an element of an untracked array, a conditional, a local the replay never
//     recorded) is not modelled: it may name any tracked object, so the answer is every tracked object,
//     not exact. The life rule then never treats the copy as safe only because it could not follow it.
// ES: Objetos rastreados que puede nombrar el valor de referencia `value` cuando el replay lo copia a un
//     elemento o a un local (doc 20). Literales, `null`, un objeto nuevo, `this`, un parametro y un move
//     (cuyo efecto de ownership se sigue en otro lugar) no nombran objetos rastreados. Un dueno rastreado
//     nombra sus objetos y un local no dueno registrado nombra lo registrado para el; ambos son exactos
//     (el valor es uno de esos objetos). La lectura de un elemento de un arreglo local rastreado nombra
//     los candidatos de ese elemento, pero el elemento puede guardar tambien un objeto no rastreado, asi
//     que la respuesta no es exacta. Todo lo demas (un resultado de llamada, un miembro, un elemento de un
//     arreglo no rastreado, un condicional, un local que el replay nunca registro) no se modela: puede
//     nombrar cualquier objeto rastreado, asi que la respuesta es todo objeto rastreado, no exacta. La
//     regla de vida nunca trata entonces la copia como segura solo porque no pudo seguirla.
// Complexity: O(tracked objects + slots). Effects: none.
ArrayReferenceValueObjects ProjectMethodBodyModel::arrayReferenceValueObjects(
    const ast::AstNode* value, std::size_t scopeOrdinal, std::size_t methodMemberOrdinal)
{
    const auto everything = [&]
    {
        ArrayReferenceValueObjects all;
        all.exact = false;
        for (const auto& [id, life] : arrayReferenceFlow_.objects)
        {
            (void)life;
            all.objects.insert(id);
        }
        for (const auto& [owner, ids] : arrayReferenceFlow_.owners)
        {
            (void)owner;
            all.objects.insert(ids.begin(), ids.end());
        }
        return all;
    };
    if (value == nullptr) return {};
    switch (value->kind())
    {
    case ast::AstNodeKind::NullLiteralExpression:
    case ast::AstNodeKind::IntegerLiteralExpression:
    case ast::AstNodeKind::FloatingLiteralExpression:
    case ast::AstNodeKind::CharacterLiteralExpression:
    case ast::AstNodeKind::StringLiteralExpression:
    case ast::AstNodeKind::BooleanLiteralExpression:
    case ast::AstNodeKind::DefaultValueExpression:
    case ast::AstNodeKind::NewExpression:
    case ast::AstNodeKind::ObjectCreationExpression:
    case ast::AstNodeKind::ThisExpression:
    case ast::AstNodeKind::MoveExpression:
        return {};
    case ast::AstNodeKind::IdentifierExpression:
    {
        const auto name = lookupScopeName(scopeOrdinal, value->text());
        if (!name.has_value()) return everything();
        if (name->kind == ScopeNameKind::Parameter) return {};
        if (name->kind != ScopeNameKind::Local || name->semanticOrdinal >= locals_.size()) return everything();
        if (const auto owner = arrayReferenceFlow_.owners.find(name->semanticOrdinal);
            owner != arrayReferenceFlow_.owners.end())
            return {owner->second, true};
        if (arrayReferenceFlow_.arrayForLocal(name->semanticOrdinal).has_value()) return {};
        if (const auto recorded = arrayReferenceValueLocals_.find(name->semanticOrdinal);
            recorded != arrayReferenceValueLocals_.end())
            return recorded->second;
        if (!isReferenceTypeKey(locals_[name->semanticOrdinal].typeKey)) return {};
        return everything();
    }
    case ast::AstNodeKind::IndexExpression:
    {
        const ast::AstNode* base = value->first();
        const auto array = base != nullptr && base->kind() == ast::AstNodeKind::IdentifierExpression
            ? lookupScopeName(scopeOrdinal, base->text()) : std::optional<ScopeName>{};
        const auto root = array && array->kind == ScopeNameKind::Local
            ? arrayReferenceFlow_.arrayForLocal(array->semanticOrdinal) : std::nullopt;
        if (!root) return everything();
        const ast::AstNode* index = value->children().size() == 1
            ? value->children().front()
            : (value->children().empty() ? value->second() : nullptr);
        const auto constant = index != nullptr
            ? storageIntegerConstant(index, scopeOrdinal, methodMemberOrdinal)
            : std::optional<std::int64_t>{};
        return {arrayReferenceFlow_.useCandidates({*root, constant}), false};
    }
    default:
        return everything();
    }
}

// EN: Records (`replace`) or widens what the non-owning reference local `localOrdinal` may name after it
//     receives `value` (doc 20). A widened record is the union and stays exact only when both parts are.
//     Owners, arrays, `ref` locals, statics and non-reference locals follow their own rules and are not
//     recorded.
// ES: Registra (`replace`) o amplia lo que puede nombrar el local de referencia no dueno `localOrdinal`
//     despues de recibir `value` (doc 20). Un registro ampliado es la union y solo sigue exacto si ambas
//     partes lo son. Duenos, arreglos, locales `ref`, estaticos y locales que no son de referencia siguen
//     sus propias reglas y no se registran.
// Complexity: O(tracked objects + slots). Effects: arrayReferenceValueLocals_.
void ProjectMethodBodyModel::recordArrayReferenceValueLocal(
    std::size_t localOrdinal, const ast::AstNode* value, std::size_t scopeOrdinal,
    std::size_t methodMemberOrdinal, bool replace)
{
    if (arrayReferenceReplaySource_ == nullptr || localOrdinal >= locals_.size()) return;
    const MethodLocalSymbol& local = locals_[localOrdinal];
    if (local.isRaii || local.isStatic || local.isReference || !isReferenceTypeKey(local.typeKey) ||
        ProjectTypeConversion::isNativeArray(local.typeKey) || arrayReferenceFlow_.owners.contains(localOrdinal))
        return;
    ArrayReferenceValueObjects named = arrayReferenceValueObjects(value, scopeOrdinal, methodMemberOrdinal);
    const auto recorded = arrayReferenceValueLocals_.find(localOrdinal);
    if (replace || recorded == arrayReferenceValueLocals_.end())
    {
        arrayReferenceValueLocals_[localOrdinal] = std::move(named);
        return;
    }
    recorded->second.objects.insert(named.objects.begin(), named.objects.end());
    recorded->second.exact = recorded->second.exact && named.exact;
}
}
