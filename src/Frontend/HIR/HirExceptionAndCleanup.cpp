#include "HirLoweringSupport.h"

#include <unordered_set>
#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
std::optional<std::size_t> ProjectOopHirModel::cleanupRegionOrdinal(
    const MethodCleanupRegion* region) const
{
    if (region == nullptr || methodBodyModel_ == nullptr) return std::nullopt;
    const auto regions = methodBodyModel_->cleanupRegions();
    if (regions.empty() || region < regions.data() || region >= regions.data() + regions.size())
        return std::nullopt;
    return static_cast<std::size_t>(region - regions.data());
}

void ProjectOopHirModel::applyStructuredExitBinding(
    ProjectOopHirNode& node,
    const ast::AstNode* syntax) const
{
    const MethodStructuredExitBinding* binding = structuredExitBinding(syntax);
    if (binding == nullptr) return;
    node.cleanupRegionOrdinals = binding->cleanupRegionOrdinals;
    node.targetScopeOrdinal = binding->targetScopeOrdinal;
    switch (binding->kind)
    {
        case MethodStructuredExitKind::Return:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            break;
        case MethodStructuredExitKind::Break:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Break;
            break;
        case MethodStructuredExitKind::Continue:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Continue;
            break;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFullExpressionCleanup(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* syntax,
    bool hasOwnedTemporary)
{
    if (!hasOwnedTemporary) return value;
    auto fullExpression = makeNode(ProjectOopHirNodeKind::FullExpressionCleanup, syntax, value->typeKey);
    fullExpression->children.push_back(std::move(value));
    return fullExpression;
}

// EN: Freeze C-11 (found while probing value lifetimes): the result of a call that hands its
//     caller a value to destroy (an owned object or string, or a struct with a destructor or
//     owned fields) leaked when it was discarded (`Make();`) or only used as a receiver
//     (`Make().Count`, `Make().Id()`). Such a temporary now gets a hidden owner that the full
//     expression destroys. A receiver is owned only when its consumer yields a plain scalar,
//     because a reference or a string taken from the temporary could outlive it.
// ES: Congelamiento C-11 (hallazgo al sondear lifetimes de valores): el resultado de una
//     llamada que entrega al llamador un valor que destruir (un objeto o string owned, o un
//     struct con destructor o campos owned) se fugaba cuando se descartaba (`Make();`) o solo se
//     usaba como receptor (`Make().Count`, `Make().Id()`). Ese temporal recibe ahora un dueno
//     oculto que la expresion completa destruye. Un receptor solo se posee cuando su consumidor
//     produce un escalar simple, porque una referencia o un string tomado del temporal podria
//     sobrevivirle.
namespace
{
bool hasPlainResult(const ProjectOopHirNode& node)
{
    if (node.referenceResult) return false;
    const std::string& type = node.typeKey;
    return type == "builtin:void" ||
        (type.starts_with("builtin:") && type != "builtin:string" && !type.ends_with("*") &&
         !type.ends_with("[]"));
}

bool consumesReceiver(const ProjectOopHirNode& node)
{
    return !node.children.empty() &&
        (node.kind == ProjectOopHirNodeKind::InstanceCall ||
         node.kind == ProjectOopHirNodeKind::FieldValue ||
         node.kind == ProjectOopHirNodeKind::StringLength ||
         node.kind == ProjectOopHirNodeKind::StringElementValue);
}

// EN: Freeze (plan item 131a): helpers of ownReceiversUntilFullExpression. A statement node ends a full
//     expression; so does a node that evaluates an operand only on some paths or suspends, because a cleanup
//     closed past it would destroy a temporary that one path never made (or keep it across a suspension).
// ES: Congelamiento (elemento 131a del plan): auxiliares de ownReceiversUntilFullExpression. Un nodo de
//     sentencia termina una expresion completa; tambien un nodo que evalua un operando solo en algunos caminos o
//     que suspende, porque una limpieza cerrada despues de el destruiria un temporal que un camino nunca creo (o
//     lo mantendria a traves de una suspension).
using HirParentIndex = std::unordered_map<const ProjectOopHirNode*, ProjectOopHirNode*>;

bool hirStatementKind(ProjectOopHirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopHirNodeKind::Block:
        case ProjectOopHirNodeKind::LocalDeclaration:
        case ProjectOopHirNodeKind::ExpressionStatement:
        case ProjectOopHirNodeKind::InlineAssembly:
        case ProjectOopHirNodeKind::If:
        case ProjectOopHirNodeKind::While:
        case ProjectOopHirNodeKind::DoWhile:
        case ProjectOopHirNodeKind::For:
        case ProjectOopHirNodeKind::Switch:
        case ProjectOopHirNodeKind::SwitchSection:
        case ProjectOopHirNodeKind::Return:
        case ProjectOopHirNodeKind::TryRegion:
        case ProjectOopHirNodeKind::CatchRegion:
        case ProjectOopHirNodeKind::FinallyRegion:
        case ProjectOopHirNodeKind::UsingRegion:
        case ProjectOopHirNodeKind::Throw:
        case ProjectOopHirNodeKind::YieldValue:
        case ProjectOopHirNodeKind::AsyncYieldValue:
            return true;
        default:
            return false;
    }
}

bool hirConditionalEvaluation(const ProjectOopHirNode& node) noexcept
{
    switch (node.kind)
    {
        case ProjectOopHirNodeKind::Conditional:
        case ProjectOopHirNodeKind::NullCoalescing:
        case ProjectOopHirNodeKind::OptionalCoalescing:
        case ProjectOopHirNodeKind::NullCoalescingAssignment:
        case ProjectOopHirNodeKind::NullConditionalFieldValue:
        case ProjectOopHirNodeKind::NullConditionalFieldStore:
        case ProjectOopHirNodeKind::NullConditionalArrayLoad:
        case ProjectOopHirNodeKind::NullConditionalArrayStore:
        case ProjectOopHirNodeKind::LoopExpression:
        case ProjectOopHirNodeKind::MatchExpression:
        case ProjectOopHirNodeKind::MatchArm:
        case ProjectOopHirNodeKind::BlockExpression:
        case ProjectOopHirNodeKind::LambdaCreate:
        case ProjectOopHirNodeKind::AwaitFuture:
        case ProjectOopHirNodeKind::SuspendPoint:
        case ProjectOopHirNodeKind::ResultPropagation:
            return true;
        case ProjectOopHirNodeKind::Binary:
            return node.operation == lexer::TokenKind::AmpAmp || node.operation == lexer::TokenKind::PipePipe;
        default:
            return false;
    }
}

bool hirSubtreeSuspends(const ProjectOopHirNode& root)
{
    std::vector<const ProjectOopHirNode*> pending{&root};
    while (!pending.empty())
    {
        const ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopHirNodeKind::AwaitFuture || node->kind == ProjectOopHirNodeKind::SuspendPoint ||
            node->kind == ProjectOopHirNodeKind::YieldValue || node->kind == ProjectOopHirNodeKind::AsyncYieldValue)
            return true;
        for (const std::unique_ptr<ProjectOopHirNode>& child : node->children) pending.push_back(child.get());
    }
    return false;
}

// EN: Parent of every node under `root`, and the nodes in source order (preorder, left to right).
// ES: Padre de cada nodo bajo `root`, y los nodos en orden de fuente (preorden, de izquierda a derecha).
void indexHirParents(ProjectOopHirNode* root, HirParentIndex& parents, std::vector<ProjectOopHirNode*>& order)
{
    std::vector<ProjectOopHirNode*> pending{root};
    while (!pending.empty())
    {
        ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        order.push_back(node);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
        {
            if (*child == nullptr) continue;
            parents[child->get()] = node;
            pending.push_back(child->get());
        }
    }
}

std::unique_ptr<ProjectOopHirNode>* hirChildSlot(ProjectOopHirNode& parent, const ProjectOopHirNode* child)
{
    for (std::unique_ptr<ProjectOopHirNode>& slot : parent.children)
        if (slot.get() == child) return &slot;
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode>* hirReceiverSlot(ProjectOopHirNode& consumer)
{
    if (consumer.children.empty() || consumer.children.front() == nullptr) return nullptr;
    std::unique_ptr<ProjectOopHirNode>* receiver = &consumer.children.front();
    if ((*receiver)->kind == ProjectOopHirNodeKind::TemporaryAddress && (*receiver)->children.size() == 1)
        receiver = &(*receiver)->children.front();
    return *receiver != nullptr ? receiver : nullptr;
}

// EN: Freeze (plan item 135): `borrowed` is a value operand of a `?:` (not its condition) or an operand of a
//     `??`, so the borrow reaches the result on some path.
// ES: Congelamiento (elemento 135 del plan): `borrowed` es un operando de valor de un `?:` (no su condicion)
//     o un operando de un `??`, asi que el prestamo llega al resultado en algun camino.
bool hirBranchJoin(const ProjectOopHirNode& consumer, const ProjectOopHirNode& borrowed) noexcept
{
    if (consumer.kind == ProjectOopHirNodeKind::NullCoalescing) return true;
    return consumer.kind == ProjectOopHirNodeKind::Conditional && !consumer.children.empty() &&
        consumer.children.front().get() != &borrowed;
}

// EN: A node that would keep the borrow beyond the expression, or that closes cleanups of its own.
// ES: Un nodo que guardaria el prestamo mas alla de la expresion, o que cierra limpiezas propias.
bool hirKeepsBorrow(ProjectOopHirNodeKind kind) noexcept
{
    return kind == ProjectOopHirNodeKind::FullExpressionCleanup ||
        kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow ||
        kind == ProjectOopHirNodeKind::Assignment ||
        kind == ProjectOopHirNodeKind::CompoundStore;
}

// EN: `borrowed` is the object whose field `consumer` stores into (`Make().Inner.V = 3`): the store ends the
//     borrow. Returns the assignment, or null.
// ES: `borrowed` es el objeto en cuyo campo escribe `consumer` (`Make().Inner.V = 3`): la escritura termina el
//     prestamo. Devuelve la asignacion, o null.
ProjectOopHirNode* hirFieldStoreThrough(
    const ProjectOopHirNode& consumer,
    const ProjectOopHirNode& borrowed,
    const HirParentIndex& parents)
{
    if (consumer.kind != ProjectOopHirNodeKind::FieldAddress || consumer.children.empty() ||
        consumer.children.front().get() != &borrowed)
        return nullptr;
    const auto parent = parents.find(&consumer);
    if (parent == parents.end() || parent->second->kind != ProjectOopHirNodeKind::Assignment ||
        parent->second->children.empty() || parent->second->children.front().get() != &consumer)
        return nullptr;
    return parent->second;
}

// EN: Freeze (plan item 135a): `argument` is an owned temporary lent to a parameter that does not take
//     ownership (its hidden owner destroys it at the end of the full expression).
// ES: Congelamiento (elemento 135a del plan): `argument` es un temporal owned prestado a un parametro que no
//     toma ownership (su dueno oculto lo destruye al final de la expresion completa).
bool hirLentTemporaryArgument(const ProjectOopHirNode& argument) noexcept
{
    if (argument.kind != ProjectOopHirNodeKind::OwnedTemporaryBorrow || argument.booleanValue ||
        argument.children.size() != 1U || argument.children.front() == nullptr)
        return false;
    const ProjectOopHirNode& borrowed = *argument.children.front();
    return borrowed.kind == ProjectOopHirNodeKind::BorrowValue &&
        borrowed.borrowKind == MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
}
}

// EN: X2-05. A non-trivial C++ object built only to lend its address (`o = lib.make(42)` passes it to operator=,
//     a `const T&` argument) is constructed in a hidden owner's storage and destroyed at the end of the enclosing
//     full expression, as a C++ temporary. The owner's drop flag is set after the construction returns, so a path
//     that never built it (a `?:` branch, an exception) destroys nothing.
// ES: X2-05. Un objeto C++ no trivial construido solo para prestar su direccion se construye en el almacenamiento de
//     un dueno oculto y se destruye al final de la expresion completa que lo contiene, como un temporal C++. El
//     indicador de destruccion del dueno se activa al volver la construccion, asi que un camino que no lo construyo
//     no destruye nada.
void ProjectOopHirModel::ownCppTemporaries()
{
    if (nameLookup_ == nullptr) return;
    // X2-05: NativeSharp functions whose result is a non-trivial C++ record construct it in the caller's storage.
    //     / Las funciones NativeSharp cuyo resultado es un record C++ no trivial lo construyen en el destino.
    for (ProjectOopHirFunction& function : functions_)
    {
        // X2-05: a C++ method marked so constructs its result in the destination too. / Un metodo C++ asi marcado
        //     tambien construye su resultado en el destino.
        if (function.isCppExternal && memberModel_ != nullptr &&
            function.memberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[function.memberOrdinal].cppConstructedResult)
        {
            function.cppConstructedResult = true;
            continue;
        }
        if (function.isCppExternal || function.referenceReturn || !function.returnTypeKey.starts_with("type:")) continue;
        const auto ordinal = nameLookup_->typeOrdinalForIdentity(std::string_view(function.returnTypeKey).substr(5));
        const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
        function.cppConstructedResult = imported != nullptr && imported->origin == CppImportedTypeOrigin::CppRecord &&
            !nameLookup_->cppTrivialValueEligible(*ordinal);
        if (!function.cppConstructedResult || memberModel_ == nullptr) continue;
        // EN: X2-05. `return o;` of a local of that record is C++'s implicit move: the result is constructed in the
        //     caller's storage by the move constructor from o (the copy constructor when there is none, as overload
        //     resolution picks for an xvalue); o stays a live local its scope destroys.
        // ES: X2-05. `return o;` de un local de ese record es el movimiento implicito de C++: el resultado se construye
        //     en el almacenamiento del llamador con el constructor de movimiento desde o (el de copia si no hay); o
        //     sigue siendo un local vivo que su ambito destruye.
        std::optional<std::size_t> moveConstructor;
        std::optional<std::size_t> copyConstructor;
        for (const std::size_t member : memberModel_->findMembers(*ordinal, ".ctor"))
        {
            if (member >= memberModel_->members().size()) continue;
            const auto& constructor = memberModel_->members()[member];
            if (!constructor.isCppExternal || constructor.cppDeleted || constructor.cppFactoryConstructor ||
                constructor.parameters.size() != 1 ||
                constructor.accessibility != ClassMemberAccessibility::Public) continue;
            const auto& parameter = constructor.parameters[0];
            if (parameter.pointeeTypeKey != function.returnTypeKey) continue;
            if (parameter.passKind == ParameterPassKind::Ref && parameter.cppRvalueReference) moveConstructor = member;
            else if (parameter.passKind == ParameterPassKind::In) copyConstructor = member;
        }
        const auto selected = moveConstructor ? moveConstructor : copyConstructor;
        if (!selected) continue;
        std::vector<ProjectOopHirNode*> pending{function.body.get()};
        while (!pending.empty())
        {
            ProjectOopHirNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr || node->kind == ProjectOopHirNodeKind::LambdaCreate) continue;
            for (const auto& child : node->children) pending.push_back(child.get());
            if (node->kind != ProjectOopHirNodeKind::Return || node->children.size() != 1 ||
                node->children[0] == nullptr) continue;
            // `return move(o);` is the same move in C++; o, moved from, is still destroyed by its scope.
            // `return move(o);` es el mismo movimiento en C++; o, ya movido, lo sigue destruyendo su ambito.
            if (node->children[0]->kind == ProjectOopHirNodeKind::MoveValue)
            {
                const auto& moved = *node->children[0];
                const std::optional<std::size_t> ordinal = moved.localOrdinal ? moved.localOrdinal
                    : moved.children.size() == 1 && moved.children[0] ? moved.children[0]->localOrdinal
                    : std::nullopt;
                if (ordinal)
                {
                    auto local = makeNode(ProjectOopHirNodeKind::LocalValue, moved.syntax, moved.typeKey);
                    local->localOrdinal = ordinal;
                    node->children[0] = std::move(local);
                }
            }
            if (node->children[0]->kind != ProjectOopHirNodeKind::LocalValue ||
                !node->children[0]->localOrdinal || node->children[0]->typeKey != function.returnTypeKey) continue;
            auto& value = node->children[0];
            const ast::AstNode* syntax = value->syntax;
            auto address = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, function.returnTypeKey);
            address->localOrdinal = value->localOrdinal;
            auto pointer = makeNode(ProjectOopHirNodeKind::AddressValue, syntax, function.returnTypeKey + "*");
            pointer->children.push_back(std::move(address));
            auto construction = makeNode(ProjectOopHirNodeKind::ValueConstruction, syntax, function.returnTypeKey);
            construction->targetTypeOrdinal = *ordinal;
            construction->memberOrdinal = *selected;
            construction->argumentCount = 1;
            construction->children.push_back(std::move(pointer));
            value = std::move(construction);
        }
    }
    const auto visit = [&](std::unique_ptr<ProjectOopHirNode>& root) {
        if (root == nullptr) return;
        // EN: X2-05. A call of a C++ factory function that analysis did not turn into a construction (an operator such
        //     as `a + b`) is the construction of its record with the function's factory constructor, as a direct call is.
        // ES: X2-05. Una llamada a una funcion fabrica C++ que el analisis no convirtio en construccion (un operador
        //     como `a + b`) es la construccion de su record con el constructor de fabrica, como una llamada directa.
        if (namespaceFunctionModel_ != nullptr && memberModel_ != nullptr)
        {
            std::vector<ProjectOopHirNode*> calls{root.get()};
            while (!calls.empty())
            {
                ProjectOopHirNode* node = calls.back();
                calls.pop_back();
                if (node == nullptr) continue;
                for (const auto& child : node->children) calls.push_back(child.get());
                if (node->kind != ProjectOopHirNodeKind::StaticCall || node->referenceResult) continue;
                // By its ordinal, or by its symbol (an operator call). / Por su ordinal, o por su simbolo.
                const NamespaceFunctionSymbol* found = nullptr;
                if (node->namespaceFunctionOrdinal &&
                    *node->namespaceFunctionOrdinal < namespaceFunctionModel_->functions().size())
                    found = &namespaceFunctionModel_->functions()[*node->namespaceFunctionOrdinal];
                else if (!node->stringValue.empty())
                    for (const auto& candidate : namespaceFunctionModel_->functions())
                        if (candidate.cppFactoryConstruction && candidate.mangledName == node->stringValue)
                            found = &candidate;
                if (found == nullptr) continue;
                const auto& factory = *found;
                if (!factory.cppFactoryConstruction || !factory.declaredTypeKey.starts_with("type:")) continue;
                const auto record = nameLookup_->typeOrdinalForIdentity(std::string_view(factory.declaredTypeKey).substr(5));
                std::optional<std::size_t> constructor;
                for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
                    if (memberModel_->members()[ordinal].cppFactoryConstructor &&
                        memberModel_->members()[ordinal].cppExternalSymbol == factory.mangledName)
                        constructor = ordinal;
                if (!record || !constructor) continue;
                node->kind = ProjectOopHirNodeKind::ValueConstruction;
                node->targetTypeOrdinal = *record;
                node->memberOrdinal = *constructor;
                node->namespaceFunctionOrdinal.reset();
                node->argumentCount = node->children.size();
            }
        }
        HirParentIndex parents;
        std::vector<ProjectOopHirNode*> order;
        indexHirParents(root.get(), parents, order);
        const auto cppConstruction = [&](const ProjectOopHirNode* value) {
            return value != nullptr && value->kind == ProjectOopHirNodeKind::ValueConstruction &&
                value->targetTypeOrdinal && nameLookup_->cppImportedType(*value->targetTypeOrdinal) != nullptr &&
                !nameLookup_->cppTrivialValueEligible(*value->targetTypeOrdinal);
        };
        // EN: X2-05. A call to a by-value adapter whose by-value record arguments are all materialized C++
        //     constructions calls the original function instead (its hidden direct variant): each prvalue is
        //     constructed in the temporary the callee receives and destroys (Microsoft x64), so the caller
        //     registers no destructor for it, as C++ constructs the parameter in place.
        // ES: X2-05. Una llamada a un adaptador por valor cuyos argumentos record por valor son todos construcciones
        //     C++ materializadas llama en su lugar a la funcion original (su variante directa oculta): cada prvalue
        //     se construye en el temporal que recibe y destruye el llamado, asi que el llamador no registra su
        //     destructor, como C++ construye el parametro en su lugar.
        // EN: An object lent with `in` (`ref` for a move-only record) is the copy (the move) C++ constructs in that
        //     parameter: it becomes that construction, built in the same temporary.
        // ES: Un objeto prestado con `in` (`ref` si solo se mueve) es la copia (el movimiento) que C++ construye en ese
        //     parametro: se vuelve esa construccion, en el mismo temporal.
        const auto lifetimeConstructor = [&](const std::string& typeKey, bool move) -> std::optional<std::size_t> {
            const auto ordinal = typeKey.starts_with("type:")
                ? nameLookup_->typeOrdinalForIdentity(std::string_view(typeKey).substr(5)) : std::nullopt;
            if (!ordinal || memberModel_ == nullptr) return std::nullopt;
            for (const std::size_t member : memberModel_->findMembers(*ordinal, ".ctor"))
            {
                if (member >= memberModel_->members().size()) continue;
                const auto& constructor = memberModel_->members()[member];
                if (!constructor.isCppExternal || constructor.cppDeleted || constructor.cppFactoryConstructor ||
                    constructor.parameters.size() != 1 ||
                    constructor.accessibility != ClassMemberAccessibility::Public) continue;
                const auto& parameter = constructor.parameters[0];
                if (parameter.pointeeTypeKey != typeKey) continue;
                if (move ? parameter.passKind == ParameterPassKind::Ref && parameter.cppRvalueReference
                         : parameter.passKind == ParameterPassKind::In) return member;
            }
            return std::nullopt;
        };
        std::unordered_set<const ProjectOopHirNode*> calleeDestroyed;
        bool rewritten = false;
        if (namespaceFunctionModel_ != nullptr)
            for (ProjectOopHirNode* node : order)
            {
                if (node->kind != ProjectOopHirNodeKind::StaticCall || !node->namespaceFunctionOrdinal) continue;
                const auto functions = namespaceFunctionModel_->functions();
                if (*node->namespaceFunctionOrdinal >= functions.size()) continue;
                const NamespaceFunctionSymbol& callee = functions[*node->namespaceFunctionOrdinal];
                if (callee.cppByValueDirectSymbol.empty()) continue;
                std::optional<std::size_t> direct;
                for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
                    if (functions[ordinal].cppByValueDirect && functions[ordinal].mangledName == callee.cppByValueDirectSymbol &&
                        functions[ordinal].projectOrdinal == callee.projectOrdinal) direct = ordinal;
                if (!direct) continue;
                std::vector<const ProjectOopHirNode*> temporaries;
                std::vector<std::pair<std::size_t, std::size_t>> lent; // argument index, constructor
                bool all = true;
                for (std::size_t index = 0; index < node->children.size(); ++index)
                {
                    const std::size_t parameter = index < node->argumentParameterOrdinals.size()
                        ? node->argumentParameterOrdinals[index] : index;
                    if (parameter >= callee.parameters.size() || !callee.parameters[parameter].cppByValueRecord) continue;
                    const ProjectOopHirNode* argument = node->children[index].get();
                    const auto& declared = callee.parameters[parameter];
                    if (argument != nullptr && argument->kind == ProjectOopHirNodeKind::TemporaryAddress &&
                        argument->children.size() == 1 && cppConstruction(argument->children.front().get()))
                        temporaries.push_back(argument);
                    else if (const auto constructor = argument != nullptr &&
                                 argument->typeKey == declared.pointeeTypeKey + "*"
                                 ? lifetimeConstructor(declared.pointeeTypeKey, declared.cppRvalueReference)
                                 : std::nullopt)
                        lent.emplace_back(index, *constructor);
                    else
                        all = false;
                }
                if (!all || (temporaries.empty() && lent.empty())) continue;
                for (const auto& [index, constructor] : lent)
                {
                    auto& argument = node->children[index];
                    const ast::AstNode* syntax = argument->syntax;
                    const std::string typeKey = argument->typeKey.substr(0, argument->typeKey.size() - 1);
                    auto construction = makeNode(ProjectOopHirNodeKind::ValueConstruction, syntax, typeKey);
                    construction->targetTypeOrdinal =
                        nameLookup_->typeOrdinalForIdentity(std::string_view(typeKey).substr(5));
                    construction->memberOrdinal = constructor;
                    construction->argumentCount = 1;
                    construction->children.push_back(std::move(argument));
                    auto temporary = makeNode(ProjectOopHirNodeKind::TemporaryAddress, syntax, typeKey);
                    temporary->elementTypeKey = typeKey;
                    temporary->children.push_back(std::move(construction));
                    temporaries.push_back(temporary.get());
                    argument = std::move(temporary);
                    rewritten = true;
                }
                node->namespaceFunctionOrdinal = *direct;
                calleeDestroyed.insert(temporaries.begin(), temporaries.end());
            }
        if (rewritten)
        {
            parents.clear();
            order.clear();
            indexHirParents(root.get(), parents, order);
        }
        for (ProjectOopHirNode* node : order)
        {
            // A materialized temporary, or a construction whose address is taken directly.
            // Un temporal materializado, o una construccion cuya direccion se toma directamente.
            const auto parent = parents.find(node);
            if (parent == parents.end()) continue;
            // X2-05: or the call of a NativeSharp function that constructs its C++ result, a receiver temporary
            //     (`Make(4).v`). / O la llamada de una funcion NativeSharp que construye su resultado C++.
            const auto constructedCall = [&](const ProjectOopHirNode* value) {
                if (value == nullptr || value->referenceResult) return false;
                const ProjectOopHirFunction* called = nullptr;
                if (value->kind == ProjectOopHirNodeKind::StaticCall && value->namespaceFunctionOrdinal)
                    called = functionForNamespaceFunction(*value->namespaceFunctionOrdinal);
                else if ((value->kind == ProjectOopHirNodeKind::StaticCall ||
                          value->kind == ProjectOopHirNodeKind::InstanceCall) && value->memberOrdinal)
                    called = functionForMember(*value->memberOrdinal);
                return called != nullptr && called->cppConstructedResult;
            };
            // X2-05: the address of an owned temporary that holds such a call's result is that owner's slot: the
            //     result is constructed there. / La direccion de un temporal con dueno que guarda ese resultado es la
            //     ranura del dueno: el resultado se construye alli.
            if (node->kind == ProjectOopHirNodeKind::TemporaryAddress && node->children.size() == 1 &&
                node->children.front() && node->children.front()->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow &&
                node->children.front()->children.size() == 1 &&
                (constructedCall(node->children.front()->children.front().get()) ||
                 cppConstruction(node->children.front()->children.front().get())))
            {
                const auto parent = parents.find(node);
                std::unique_ptr<ProjectOopHirNode>* slot =
                    parent != parents.end() ? hirChildSlot(*parent->second, node) : nullptr;
                if (slot != nullptr)
                {
                    auto owner = std::move(node->children.front());
                    owner->referenceResult = true;
                    owner->automaticDestruction = true;
                    ProjectOopHirNode* ownerNode = owner.get();
                    *slot = std::move(owner);
                    parents[ownerNode] = parent->second;
                    continue;
                }
            }
            const bool materialized = node->kind == ProjectOopHirNodeKind::TemporaryAddress &&
                node->children.size() == 1 && (cppConstruction(node->children.front().get()) ||
                                               constructedCall(node->children.front().get()));
            // A non-trivial C++ record argument of a call always travels by address (`const T&`, `T&&`, the self
            //     of a member adapter). / Un argumento record C++ no trivial de una llamada viaja por direccion.
            //     A by-value adapter (`@nsc-byvalue`) would copy the prvalue where C++ constructs the parameter
            //     directly: that call keeps its rejection. / Un adaptador por valor copiaria el prvalue donde C++
            //     construye el parametro directamente: esa llamada conserva su rechazo.
            const auto* callee = parent->second->kind == ProjectOopHirNodeKind::StaticCall &&
                    parent->second->namespaceFunctionOrdinal && namespaceFunctionModel_ != nullptr &&
                    *parent->second->namespaceFunctionOrdinal < namespaceFunctionModel_->functions().size()
                ? &namespaceFunctionModel_->functions()[*parent->second->namespaceFunctionOrdinal] : nullptr;
            // Only for a parameter that takes an address: a C record or union by value travels as its bytes.
            //     / Solo para un parametro que toma una direccion: un record o union C por valor viaja como bytes.
            const auto byAddressParameter = [&]() {
                const auto& arguments = parent->second->children;
                const auto position = std::find_if(arguments.begin(), arguments.end(),
                    [&](const auto& child) { return child.get() == node; });
                if (position == arguments.end()) return false;
                const auto index = static_cast<std::size_t>(position - arguments.begin());
                const std::size_t ordinal = index < parent->second->argumentParameterOrdinals.size()
                    ? parent->second->argumentParameterOrdinals[index] : index;
                if (ordinal >= callee->parameters.size()) return false;
                const auto& parameter = callee->parameters[ordinal];
                return parameter.passKind != ParameterPassKind::Value || parameter.cppConstReference ||
                    parameter.cppRvalueReference || parameter.cppImplicitObject;
            };
            const bool addressed = cppConstruction(node) &&
                (parent->second->kind == ProjectOopHirNodeKind::AddressValue ||
                 (callee != nullptr && !(callee->cppClangUsr && callee->cppClangUsr->ends_with("@nsc-byvalue")) &&
                  byAddressParameter()));
            if (!materialized && !addressed) continue;
            std::unique_ptr<ProjectOopHirNode>* slot = hirChildSlot(*parent->second, node);
            if (slot == nullptr) continue;
            const bool destroyedByCallee = calleeDestroyed.contains(node);
            auto owner = ownTemporary(materialized ? std::move(node->children.front()) : std::move(*slot));
            owner->referenceResult = true;
            // X2-05: a by-value parameter's temporary keeps its cleanup until its call, which clears its drop flag
            //     (the callee destroys it). / Conserva su limpieza hasta su llamada, que apaga su indicador.
            owner->automaticDestruction = true;
            owner->cppCalleeDestroyed = destroyedByCallee;
            ProjectOopHirNode* ownerNode = owner.get();
            *slot = std::move(owner);
            parents[ownerNode] = parent->second;
            // The full expression: the child of the nearest statement, unless a cleanup already closes it.
            // La expresion completa: el hijo de la sentencia mas cercana, salvo que una limpieza ya la cierre.
            ProjectOopHirNode* expressionRoot = ownerNode;
            bool closed = false;
            while (true)
            {
                const auto up = parents.find(expressionRoot);
                if (up == parents.end()) break;
                if (up->second->kind == ProjectOopHirNodeKind::FullExpressionCleanup) { closed = true; break; }
                if (hirStatementKind(up->second->kind)) break;
                expressionRoot = up->second;
            }
            if (closed) continue;
            const auto statement = parents.find(expressionRoot);
            if (statement == parents.end()) continue;
            std::unique_ptr<ProjectOopHirNode>* expressionSlot = hirChildSlot(*statement->second, expressionRoot);
            if (expressionSlot == nullptr) continue;
            const ast::AstNode* syntax = expressionRoot->syntax;
            *expressionSlot = wrapFullExpressionCleanup(std::move(*expressionSlot), syntax, true);
            // An address result (`T& operator=`) stays an address. / Un resultado direccion sigue siendolo.
            (*expressionSlot)->referenceResult = expressionRoot->referenceResult;
            parents[expressionSlot->get()] = statement->second;
            parents[expressionRoot] = expressionSlot->get();
        }
    };
    for (ProjectOopHirFunction& function : functions_) visit(function.body);
}

bool ProjectOopHirModel::isFreshDroppableTemporary(const ProjectOopHirNode* node) const
{
    if (node == nullptr || node->referenceResult || memberModel_ == nullptr) return false;
    const bool producer = node->kind == ProjectOopHirNodeKind::InstanceCall ||
        node->kind == ProjectOopHirNodeKind::StaticCall ||
        node->kind == ProjectOopHirNodeKind::DelegateInvoke ||
        (node->kind == ProjectOopHirNodeKind::CompoundStore && node->booleanValue &&
         node->memberOrdinal.has_value() && isReferenceType(node->typeKey)) ||
        node->kind == ProjectOopHirNodeKind::StringConcat ||
        node->kind == ProjectOopHirNodeKind::StringSubstring;
    if (!producer) return false;
    if (memberModel_->typeRequiresValueDrop(node->typeKey)) return true;
    if (!isReferenceType(node->typeKey) && node->typeKey != "builtin:string") return false;
    const MethodExpressionTypeBinding* binding = expressionType(node->syntax);
    return binding != nullptr && binding->ownership == sema::OwnershipKind::Owned &&
        binding->ownershipTransfer && !binding->ownershipSourceLocalOrdinal.has_value() &&
        !binding->ownershipSourceParameterOrdinal.has_value();
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::ownTemporary(
    std::unique_ptr<ProjectOopHirNode> value)
{
    auto temporary = makeNode(ProjectOopHirNodeKind::OwnedTemporaryBorrow, value->syntax, value->typeKey);
    temporary->localOrdinal = nextSyntheticLocalOrdinal_++;
    temporary->cleanupRegionOrdinal = nextSyntheticCleanupRegionOrdinal_++;
    temporary->children.push_back(std::move(value));
    return temporary;
}

bool ProjectOopHirModel::ownReceiverTemporary(std::unique_ptr<ProjectOopHirNode>& consumer)
{
    // EN: Freeze (receiver temporaries): a consumer whose result is plain data (a struct of
    //     scalars, say) copies it out of the temporary, so the temporary is owned and destroyed
    //     too; before, only a scalar result was, and `Make().Inner` leaked the temporary.
    // ES: Congelamiento (temporales receptores): un consumidor cuyo resultado son datos
    //     simples (un struct de escalares, por ejemplo) los copia fuera del temporal, asi que el
    //     temporal tambien se posee y se destruye; antes solo con un resultado escalar, y
    //     `Make().Inner` perdia el temporal.
    const bool plainDataResult = !consumer->referenceResult && memberModel_ != nullptr &&
        memberModel_->typeIsPlainData(consumer->typeKey);
    if (!consumesReceiver(*consumer) || (!hasPlainResult(*consumer) && !plainDataResult)) return false;
    // EN: X2-05. A C++ object returned by value and used only as a receiver (`lib.make(42).v`, `lib.make(1).get()`)
    //     is constructed in the hidden owner's storage, lent by address, and destroyed at the end of the full
    //     expression, as a C++ temporary; it is never copied.
    // ES: X2-05. Un objeto C++ devuelto por valor y usado solo como receptor se construye en el almacenamiento del
    //     dueno oculto, se presta por direccion y se destruye al final de la expresion completa, como un temporal
    //     C++; nunca se copia.
    if (std::unique_ptr<ProjectOopHirNode>& address = consumer->children.front();
        address != nullptr && address->kind == ProjectOopHirNodeKind::TemporaryAddress &&
        address->children.size() == 1 && address->children.front() != nullptr &&
        address->children.front()->kind == ProjectOopHirNodeKind::ValueConstruction &&
        address->children.front()->targetTypeOrdinal && nameLookup_ != nullptr &&
        nameLookup_->cppImportedType(*address->children.front()->targetTypeOrdinal) != nullptr &&
        !nameLookup_->cppTrivialValueEligible(*address->children.front()->targetTypeOrdinal))
    {
        auto owner = ownTemporary(std::move(address->children.front()));
        owner->referenceResult = true;
        owner->automaticDestruction = true;
        address = std::move(owner);
        const ast::AstNode* syntax = consumer->syntax;
        consumer = wrapFullExpressionCleanup(std::move(consumer), syntax, true);
        return true;
    }
    std::unique_ptr<ProjectOopHirNode>* receiver = &consumer->children.front();
    if (*receiver != nullptr && (*receiver)->kind == ProjectOopHirNodeKind::TemporaryAddress &&
        (*receiver)->children.size() == 1)
        receiver = &(*receiver)->children.front();
    if (!isFreshDroppableTemporary(receiver->get())) return false;
    *receiver = ownTemporary(std::move(*receiver));
    const ast::AstNode* syntax = consumer->syntax;
    consumer = wrapFullExpressionCleanup(std::move(consumer), syntax, true);
    return true;
}

void ProjectOopHirModel::ownDroppableTemporaries(ProjectOopHirNode* root)
{
    // A discarded owned tuple value lives until the end of its statement.
    const auto discardedTupleNeedsDrop = [this](const ProjectOopHirNode* value)
    {
        if (value == nullptr || value->syntax == nullptr || memberModel_ == nullptr ||
            !memberModel_->typeRequiresValueDrop(value->typeKey)) return false;
        if (value->kind == ProjectOopHirNodeKind::AggregateInitializer &&
            value->syntax->kind() == ast::AstNodeKind::TupleExpression) return true;
        return value->kind == ProjectOopHirNodeKind::Conditional &&
            ProjectTypeConversion::tupleElementTypeKeys(value->typeKey).has_value() &&
            memberModel_->typeIsMoveOnly(value->typeKey);
    };
    std::vector<ProjectOopHirNode*> pending{root};
    while (!pending.empty())
    {
        ProjectOopHirNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr) continue;
        for (std::unique_ptr<ProjectOopHirNode>& child : node->children)
        {
            if (child == nullptr) continue;
            // EN: Plan item 118: a call with an owned argument comes wrapped in its full expression; the
            //     discarded result is the call inside it.
            // ES: Elemento 118 del plan: una llamada con un argumento owned viene envuelta en su expresión
            //     completa; el resultado descartado es la llamada de adentro.
            std::unique_ptr<ProjectOopHirNode>* inner = &child;
            if (child->kind == ProjectOopHirNodeKind::FullExpressionCleanup && child->children.size() == 1U)
                inner = &child->children.front();
            if (inner != &child && node->kind == ProjectOopHirNodeKind::ExpressionStatement &&
                (isFreshDroppableTemporary(inner->get()) || discardedTupleNeedsDrop(inner->get())))
            {
                ProjectOopHirNode* producer = inner->get();
                *inner = ownTemporary(std::move(*inner));
                pending.push_back(producer);
                continue;
            }
            if (node->kind == ProjectOopHirNodeKind::ExpressionStatement &&
                (isFreshDroppableTemporary(child.get()) || discardedTupleNeedsDrop(child.get())))
            {
                ProjectOopHirNode* producer = child.get();
                const ast::AstNode* syntax = producer->syntax;
                child = wrapFullExpressionCleanup(ownTemporary(std::move(child)), syntax, true);
                pending.push_back(producer);
                continue;
            }
            ProjectOopHirNode* original = child.get();
            ownReceiverTemporary(child);
            pending.push_back(original);
        }
    }
    extendLentArgumentCleanups(root);
    ownReceiversUntilFullExpression(root);

    // Operator `in` operands materialized in hidden value slots must remain alive
    // until the enclosing expression completes, including the other side of &&/||.
    HirParentIndex parents;
    std::vector<ProjectOopHirNode*> order;
    indexHirParents(root, parents, order);
    for (ProjectOopHirNode* wrapper : order)
    {
        if (wrapper->kind != ProjectOopHirNodeKind::FullExpressionCleanup ||
            wrapper->children.size() != 1U || wrapper->children.front() == nullptr)
            continue;
        ProjectOopHirNode* call = wrapper->children.front().get();
        if (call->kind != ProjectOopHirNodeKind::StaticCall) continue;
        const bool hasValueOperand = std::ranges::any_of(call->children, [](const auto& child) {
            return child != nullptr && child->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow &&
                child->referenceResult && child->automaticDestruction;
        });
        if (!hasValueOperand) continue;
        const auto holderIt = parents.find(wrapper);
        if (holderIt == parents.end()) continue;
        ProjectOopHirNode* holder = holderIt->second;
        std::unique_ptr<ProjectOopHirNode>* slot = hirChildSlot(*holder, wrapper);
        if (slot == nullptr) continue;
        ProjectOopHirNode* closingRoot = call;
        ProjectOopHirNode* enclosing = nullptr;
        bool crossesBranch = false;
        for (ProjectOopHirNode* above = holder; above != nullptr; )
        {
            if (above->kind == ProjectOopHirNodeKind::FullExpressionCleanup)
            {
                enclosing = above;
                break;
            }
            if (hirStatementKind(above->kind)) break;
            if (hirConditionalEvaluation(*above))
            {
                const bool shortCircuit = above->kind == ProjectOopHirNodeKind::Binary &&
                    (above->operation == lexer::TokenKind::AmpAmp || above->operation == lexer::TokenKind::PipePipe);
                if (!shortCircuit && above->kind != ProjectOopHirNodeKind::Conditional &&
                    above->kind != ProjectOopHirNodeKind::NullCoalescing)
                    break;
                crossesBranch = true;
            }
            if (hirSubtreeSuspends(*above)) break;
            closingRoot = above;
            const auto parent = parents.find(above);
            above = parent == parents.end() ? nullptr : parent->second;
        }
        *slot = std::move(wrapper->children.front());
        parents.erase(wrapper);
        parents[call] = holder;
        closeAtFullExpression(closingRoot, enclosing, crossesBranch, parents);
    }
}

// EN: Freeze (plan item 135a). A call that lends an owned temporary argument (`Id(MakeText())`) came wrapped
//     in its own FullExpressionCleanup, so the temporary died right after the call even when the call returned it
//     and an enclosing expression still read it (`Id(MakeText()).Length`). When the result is a non-owned
//     reference, the wrapper is removed and the full expression is closed instead, as for a receiver temporary.
//     Storing that result is rejected by the analysis, so the borrow never outlives the expression.
// ES: Congelamiento (elemento 135a del plan). Una llamada que presta un temporal owned argumento
//     (`Id(MakeText())`) venia envuelta en su propio FullExpressionCleanup, asi que el temporal moria justo despues
//     de la llamada aunque la llamada lo devolviera y una expresion exterior lo siguiera leyendo
//     (`Id(MakeText()).Length`). Cuando el resultado es una referencia no owned, se quita la envoltura y se cierra
//     la expresion completa, como para un temporal receptor. Guardar ese resultado lo rechaza el analisis, asi que
//     el prestamo nunca sobrevive a la expresion.
void ProjectOopHirModel::extendLentArgumentCleanups(ProjectOopHirNode* root)
{
    HirParentMap parents;
    std::vector<ProjectOopHirNode*> order;
    indexHirParents(root, parents, order);
    for (ProjectOopHirNode* node : order)
    {
        if (!resultMayAliasLentArgument(*node)) continue;
        const auto parent = parents.find(node);
        if (parent == parents.end()) continue;
        ProjectOopHirNode* holder = parent->second;
        std::unique_ptr<ProjectOopHirNode>* slot = hirChildSlot(*holder, node);
        if (slot == nullptr) continue;
        ProjectOopHirNode* call = node->children.front().get();
        *slot = std::move(node->children.front());
        parents.erase(node);
        parents[call] = holder;
        ProjectOopHirNode* closingRoot = nullptr;
        ProjectOopHirNode* enclosing = nullptr;
        planFullExpressionClose(*call, parents, false, closingRoot, enclosing);
        closeAtFullExpression(closingRoot, enclosing, false, parents);
    }
}

bool ProjectOopHirModel::resultMayAliasLentArgument(const ProjectOopHirNode& fullExpression) const
{
    if (fullExpression.kind != ProjectOopHirNodeKind::FullExpressionCleanup || fullExpression.booleanValue ||
        fullExpression.children.size() != 1U || fullExpression.children.front() == nullptr)
        return false;
    const ProjectOopHirNode& call = *fullExpression.children.front();
    if (call.referenceResult || isFreshDroppableTemporary(&call)) return false;
    if (!isReferenceType(call.typeKey) && call.typeKey != "builtin:string") return false;
    for (const std::unique_ptr<ProjectOopHirNode>& argument : call.children)
    {
        if (argument != nullptr && hirLentTemporaryArgument(*argument)) return true;
    }
    return false;
}

// EN: Freeze (plan item 131a). Three receiver temporaries leaked: a field read that is not plain data
//     (`Make().Name.Length`), a call that returns a new owner (`Make().Next().V`) and a field store target
//     (`Make().V = 12;`). Each one now gets a hidden owner that the full expression destroys, after every other
//     use and in reverse order of creation. A borrowed read is owned only when its value ends in a plain value
//     (or is discarded): storing it is rejected by the analysis, and any other flow keeps the old behavior, so
//     no reference outlives the temporary.
// ES: Congelamiento (elemento 131a del plan). Tres temporales receptores se fugaban: una lectura de campo que
//     no son datos simples (`Make().Name.Length`), una llamada que devuelve un owner nuevo (`Make().Next().V`) y
//     el destino de una escritura de campo (`Make().V = 12;`). Cada uno recibe un dueno oculto que la expresion
//     completa destruye, despues de todo otro uso y en orden inverso de creacion. Una lectura prestada se posee
//     solo cuando su valor termina en un valor simple (o se descarta): guardarla lo rechaza el analisis, y otro
//     flujo conserva la conducta anterior, asi que ninguna referencia sobrevive al temporal.
//
// EN: Freeze (plan item 135). The same hidden owner now covers every form the analysis tracks with the
//     temporary's provenance: a borrow that goes on through a non-owned call or getter result, a `?:` or a `??`,
//     and `(new T()).Field`. A borrow that crosses a `?:` or `??` comes from a temporary made on one path only, so
//     its full expression clears the owner slot when it starts and the cleanup skips a temporary never made.
//     That needs a pointer owner and no suspension inside the full expression; otherwise the old behavior stays.
// ES: Congelamiento (elemento 135 del plan). El mismo dueno oculto cubre ahora toda forma que el analisis
//     sigue con la procedencia del temporal: un prestamo que continua por el resultado no owned de una llamada o
//     de un getter, por un `?:` o un `??`, y `(new T()).Campo`. Un prestamo que cruza un `?:` o `??` viene de un
//     temporal creado en un solo camino, asi que su expresion completa limpia el slot del dueno al empezar y la
//     limpieza omite el temporal que nunca se creo. Eso exige un dueno puntero y ninguna suspension dentro de la
//     expresion completa; si no, queda la conducta anterior.
void ProjectOopHirModel::ownReceiversUntilFullExpression(ProjectOopHirNode* root)
{
    HirParentMap parents;
    std::vector<ProjectOopHirNode*> order;
    indexHirParents(root, parents, order);
    for (ProjectOopHirNode* consumer : order)
    {
        bool crossesBranch = false;
        ProjectOopHirNode* start = fullExpressionTemporaryStart(*consumer, parents, crossesBranch);
        if (start == nullptr || hirSubtreeSuspends(*start)) continue;
        std::unique_ptr<ProjectOopHirNode>* receiver = hirReceiverSlot(*consumer);
        const std::string& ownerType = (*receiver)->typeKey;
        if (crossesBranch && !isReferenceType(ownerType) && ownerType != "builtin:string") continue;
        ProjectOopHirNode* closingRoot = nullptr;
        ProjectOopHirNode* enclosing = nullptr;
        if (!planFullExpressionClose(*start, parents, crossesBranch, closingRoot, enclosing)) continue;
        ProjectOopHirNode* original = receiver->get();
        ProjectOopHirNode* holder = parents.at(original);
        *receiver = ownTemporary(std::move(*receiver));
        parents[receiver->get()] = holder;
        parents[original] = receiver->get();
        closeAtFullExpression(closingRoot, enclosing, crossesBranch, parents);
    }
}

// EN: The node whose evaluation the receiver temporary of `consumer` must outlive, or null when `consumer` is
//     not one of the three shapes or its value can escape.
// ES: El nodo cuya evaluacion debe sobrevivir el temporal receptor de `consumer`, o null cuando `consumer` no es
//     una de las tres formas o su valor puede escapar.
//
// EN: Freeze (plan item 135): a `new` of a class is a temporary like an owned-returning call; an instance call
//     on one is rejected by the analysis (131b) and never reaches here. A non-owned call result follows the same
//     borrow path as a field read; `crossesBranch` reports a `?:` or `??` on that path.
// ES: Congelamiento (elemento 135 del plan): un `new` de una clase es un temporal como una llamada con
//     retorno owned; una llamada de instancia sobre el lo rechaza el analisis (131b) y nunca llega aqui. Un
//     resultado no owned de llamada sigue el mismo camino de prestamo que una lectura de campo; `crossesBranch`
//     avisa de un `?:` o `??` en ese camino.
ProjectOopHirNode* ProjectOopHirModel::fullExpressionTemporaryStart(
    ProjectOopHirNode& consumer,
    const HirParentMap& parents,
    bool& crossesBranch) const
{
    std::unique_ptr<ProjectOopHirNode>* receiver = hirReceiverSlot(consumer);
    if (receiver == nullptr) return nullptr;
    const bool newObject = isOwnedNewObject(**receiver);
    if (!newObject && !isFreshDroppableTemporary(receiver->get())) return nullptr;
    if (consumer.kind == ProjectOopHirNodeKind::FieldValue)
    {
        if (consumesIntoPlainValue(consumer)) return newObject ? &consumer : nullptr;
        return temporaryBorrowConsumer(consumer, parents, crossesBranch);
    }
    if (consumer.kind == ProjectOopHirNodeKind::InstanceCall)
    {
        const bool newOwner = isFreshDroppableTemporary(&consumer) &&
            (isReferenceType(consumer.typeKey) || consumer.typeKey == "builtin:string");
        if (newObject || consumesIntoPlainValue(consumer)) return nullptr;
        return newOwner ? &consumer : temporaryBorrowConsumer(consumer, parents, crossesBranch);
    }
    if (consumer.kind != ProjectOopHirNodeKind::FieldAddress) return nullptr;
    const auto parent = parents.find(&consumer);
    if (parent == parents.end() || parent->second->kind != ProjectOopHirNodeKind::Assignment ||
        parent->second->children.empty() || parent->second->children.front().get() != &consumer)
        return nullptr;
    return parent->second;
}

// EN: Follows a borrowed field read up through further field reads to the node that turns it into a plain
//     value (or discards it); null when the borrow could reach anything that keeps it.
// ES: Sigue una lectura de campo prestada hacia arriba por mas lecturas de campo hasta el nodo que la convierte
//     en un valor simple (o la descarta); null cuando el prestamo podria llegar a algo que lo guarde.
//
// EN: Freeze (plan item 135): the walk goes on through every node the analysis carries the provenance
//     through (passesBorrowThrough, `?:`, `??`) and also ends at a field store through the borrow or at a node
//     that makes a new owner from it (endsInNewOwner).
// ES: Congelamiento (elemento 135 del plan): el recorrido sigue por todo nodo por el que el analisis lleva la
//     procedencia (passesBorrowThrough, `?:`, `??`) y tambien termina en una escritura de campo a traves del
//     prestamo o en un nodo que hace de el un owner nuevo (endsInNewOwner).
ProjectOopHirNode* ProjectOopHirModel::temporaryBorrowConsumer(
    ProjectOopHirNode& borrowed,
    const HirParentMap& parents,
    bool& crossesBranch) const
{
    ProjectOopHirNode* current = &borrowed;
    for (auto parent = parents.find(current); parent != parents.end(); parent = parents.find(current))
    {
        ProjectOopHirNode* consumer = parent->second;
        if (consumer->kind == ProjectOopHirNodeKind::ExpressionStatement) return current;
        if (ProjectOopHirNode* store = hirFieldStoreThrough(*consumer, *current, parents)) return store;
        if (hirBranchJoin(*consumer, *current))
        {
            crossesBranch = true;
            current = consumer;
            continue;
        }
        if (hirStatementKind(consumer->kind) || hirConditionalEvaluation(*consumer) ||
            hirKeepsBorrow(consumer->kind))
            return nullptr;
        if (consumesIntoPlainValue(*consumer) || endsInNewOwner(*consumer, *current)) return consumer;
        if (!passesBorrowThrough(*consumer, *current)) return nullptr;
        current = consumer;
    }
    return nullptr;
}

// EN: Freeze (plan item 135): `new T(...)` of a class whose object and storage this expression owns (not a
//     placement or stack-promoted construction).
// ES: Congelamiento (elemento 135 del plan): `new T(...)` de una clase cuyo objeto y almacenamiento posee
//     esta expresion (no una construccion en sitio ni promovida a la pila).
bool ProjectOopHirModel::isOwnedNewObject(const ProjectOopHirNode& node) const
{
    return node.kind == ProjectOopHirNodeKind::ObjectConstruction &&
        node.objectLifetimeOwnership == sema::OwnershipKind::Owned &&
        node.storageOwnership == sema::OwnershipKind::Owned && !node.placementConstructed &&
        !node.stackPromotedObject && isReferenceType(node.typeKey);
}

// EN: Freeze (plan item 135): `consumer` makes a new owner that cannot keep `borrowed`: a new string (a
//     concatenation or any string-returning call), or an object returned by a method called on the borrow.
//     An object built from the borrow as an argument may keep it, so that one does not end the borrow.
// ES: Congelamiento (elemento 135 del plan): `consumer` crea un owner nuevo que no puede guardar `borrowed`:
//     un string nuevo (una concatenacion o cualquier llamada que devuelve string), o un objeto devuelto por un
//     metodo llamado sobre el prestamo. Un objeto construido con el prestamo como argumento puede guardarlo, asi
//     que ese no termina el prestamo.
bool ProjectOopHirModel::endsInNewOwner(const ProjectOopHirNode& consumer, const ProjectOopHirNode& borrowed) const
{
    if (!isFreshDroppableTemporary(&consumer)) return false;
    if (consumer.typeKey == "builtin:string") return true;
    return consumer.kind == ProjectOopHirNodeKind::InstanceCall && !consumer.children.empty() &&
        consumer.children.front().get() == &borrowed && isReferenceType(consumer.typeKey);
}

// EN: Freeze (plan item 135): the result of `consumer` is still a borrow of the temporary: a further
//     reference field read, a call or getter whose result is a non-owned reference, or a conversion.
// ES: Congelamiento (elemento 135 del plan): el resultado de `consumer` sigue siendo un prestamo del
//     temporal: otra lectura de campo de referencia, una llamada o getter cuyo resultado es una referencia no
//     owned, o una conversion.
bool ProjectOopHirModel::passesBorrowThrough(
    const ProjectOopHirNode& consumer,
    const ProjectOopHirNode& borrowed) const
{
    const bool referenceResult = isReferenceType(consumer.typeKey) || consumer.typeKey == "builtin:string";
    if (!referenceResult || isFreshDroppableTemporary(&consumer)) return false;
    if (consumer.kind == ProjectOopHirNodeKind::FieldValue)
        return !consumer.children.empty() && consumer.children.front().get() == &borrowed;
    return consumer.kind == ProjectOopHirNodeKind::InstanceCall ||
        consumer.kind == ProjectOopHirNodeKind::StaticCall ||
        consumer.kind == ProjectOopHirNodeKind::DelegateInvoke ||
        consumer.kind == ProjectOopHirNodeKind::Conversion;
}

bool ProjectOopHirModel::consumesIntoPlainValue(const ProjectOopHirNode& consumer) const
{
    if (consumer.referenceResult) return false;
    if (hasPlainResult(consumer)) return true;
    return memberModel_ != nullptr && memberModel_->typeIsPlainData(consumer.typeKey);
}

// EN: Wraps the full expression around `start` (up to its statement, or to the first node that evaluates
//     conditionally or suspends) in a FullExpressionCleanup, unless one already encloses it. A root that
//     contains a suspension closes at `start` instead, so no temporary is kept across it.
// ES: Envuelve la expresion completa alrededor de `start` (hasta su sentencia, o hasta el primer nodo que evalua
//     condicionalmente o suspende) en un FullExpressionCleanup, salvo que ya haya uno que la contenga. Una raiz
//     que contiene una suspension cierra en `start`, para no mantener ningun temporal a traves de ella.
//
// EN: Freeze (plan item 135): the choice is made before the receiver gets its owner
//     (planFullExpressionClose): `root` is the node to wrap, or `enclosing` the cleanup that already closes the
//     expression. A borrow that crosses a branch needs an enclosing cleanup without a suspension, because that
//     cleanup clears the owner slots when it starts (booleanValue).
// ES: Congelamiento (elemento 135 del plan): la eleccion se hace antes de dar dueno al receptor
//     (planFullExpressionClose): `root` es el nodo que se envuelve, o `enclosing` la limpieza que ya cierra la
//     expresion. Un prestamo que cruza una rama necesita una limpieza envolvente sin suspension, porque esa
//     limpieza pone en null los slots de los duenos al empezar (booleanValue).
bool ProjectOopHirModel::planFullExpressionClose(
    ProjectOopHirNode& start,
    const HirParentMap& parents,
    bool crossesBranch,
    ProjectOopHirNode*& root,
    ProjectOopHirNode*& enclosing) const
{
    root = &start;
    enclosing = nullptr;
    for (auto parent = parents.find(root); parent != parents.end(); parent = parents.find(root))
    {
        ProjectOopHirNode* above = parent->second;
        if (above->kind == ProjectOopHirNodeKind::FullExpressionCleanup)
        {
            enclosing = above;
            return !crossesBranch || !hirSubtreeSuspends(*above);
        }
        if (hirStatementKind(above->kind) || hirConditionalEvaluation(*above)) break;
        root = above;
    }
    if (root != &start && hirSubtreeSuspends(*root)) root = &start;
    return parents.contains(root);
}

void ProjectOopHirModel::closeAtFullExpression(
    ProjectOopHirNode* root,
    ProjectOopHirNode* enclosing,
    bool crossesBranch,
    HirParentMap& parents)
{
    if (enclosing != nullptr)
    {
        if (crossesBranch) enclosing->booleanValue = true;
        return;
    }
    const auto parent = parents.find(root);
    if (parent == parents.end()) return;
    ProjectOopHirNode* above = parent->second;
    std::unique_ptr<ProjectOopHirNode>* slot = hirChildSlot(*above, root);
    if (slot == nullptr) return;
    const ast::AstNode* syntax = root->syntax;
    *slot = wrapFullExpressionCleanup(std::move(*slot), syntax, true);
    (*slot)->booleanValue = crossesBranch;
    parents[slot->get()] = above;
    parents[root] = slot->get();
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExceptionOrExitStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    const auto attachStructuredExit = [&](ProjectOopHirNode& node)
    {
        applyStructuredExitBinding(node, statement);
    };

    switch (statement->kind())
    {
        case ast::AstNodeKind::BreakStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            auto node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }
        case ast::AstNodeKind::ContinueStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Continue, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }
        case ast::AstNodeKind::ReturnStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            auto node = makeNode(ProjectOopHirNodeKind::Return, statement, "builtin:void");
            attachStructuredExit(*node);
            return node;
        }
        case ast::AstNodeKind::ThrowStatement:
        {
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            const MethodExceptionBinding* binding = exceptionBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
                return nullptr;
            }
            auto node = makeNode(
                binding->kind == MethodExceptionOperationKind::Rethrow
                    ? ProjectOopHirNodeKind::Rethrow
                    : ProjectOopHirNodeKind::Throw,
                statement, "builtin:void");
            node->typeKey = binding->exceptionTypeKey.empty() ? "builtin:void" : binding->exceptionTypeKey;
            node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;

            const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                ? 0
                : activeHirExceptionHandlers_.back().retainedCleanupDepth;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
            if (!activeHirExceptionHandlers_.empty())
                node->exceptionTargetRegionOrdinal = activeHirExceptionHandlers_.back().hirRegionOrdinal;

            return node;
        }

        case ast::AstNodeKind::UsingStatement:
            return lowerUsingStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::TryStatement:
            return lowerTryStatement(statement, memberOrdinal, error);
        default:
            error = "non-exception/exit statement reached HIR exception lowering";
            return nullptr;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerArrayDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const MethodExpressionTypeBinding& operandType,
    std::string& error)
{
    if (!ProjectTypeConversion::isNativeArray(operandType.typeKey))
    {
        error = "Stage 41 delete[] requires a native dynamic array reference";
        return nullptr;
    }
    const MethodCoreBinding* elementBinding = coreBinding(expression->first());
    // EN: Freeze (owned arrays): an owned array field is addressed too, so it is cleared.
    // ES: Congelamiento (arreglos owned): un campo de arreglo owned tambien se direcciona,
    //     asi queda vacio.
    const bool clearsOwnedField = methodBodyModel_ != nullptr &&
        methodBodyModel_->arrayDeleteClearsOwnedField(expression);
    const bool directElement = clearsOwnedField || (elementBinding != nullptr &&
        (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
         elementBinding->kind == MethodCoreOperationKind::RawElement));
    auto operand = lowerExpression(expression->first(), memberOrdinal, directElement, error);
    if (operand == nullptr) return nullptr;
    auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
    node->dynamicDestruction = false;
    // The array allocation is always released by delete[]. Element ownership is a
    // separate axis carried by the operand binding. In particular, a plain T[] or
    // borrowed T[] owns no T elements even when its containing class manually
    // releases the backing buffer in its destructor.
    node->storageOwnership = operandType.storageOwnership;
    // EN: Freeze (item 130, F2): a plain local array does not own its references.
    // ES: Congelamiento (elemento 130, F2): un local arreglo plano no posee sus referencias.
    if (methodBodyModel_ != nullptr && methodBodyModel_->arrayDeleteKeepsElements(expression))
        node->storageOwnership = sema::OwnershipKind::None;
    // A jagged row is itself an owning native-array reference. Carrying the slot
    // address lets LLVM null it before recursive destruction, so an explicit row
    // delete followed by deletion of the outer array can never double-drop it.
    node->booleanValue = directElement;
    node->elementTypeKey = operandType.typeKey;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerScalarDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    const MethodExpressionTypeBinding& operandType,
    std::string& error)
{
    const bool rawPointer = operandType.typeKey.ends_with("*");
    const bool nativeString = operandType.typeKey == "builtin:string";
    const bool deletable = operandType.typeKey == "builtin:null" ||
        isReferenceType(operandType.typeKey) || rawPointer || nativeString;
    // Stage 52 generic collections: a container's cleanup code (e.g. List<T>.Clear())
    // must delete each stored element uniformly, but T may monomorphize to a plain
    // value type (int, bool, a struct, ...) that never owned heap memory in the first
    // place. Rather than forbid `delete` on such values -- which would make it
    // impossible to write ONE generic Clear()/destructor body that works for every T --
    // treat `delete <value-typed expression>;` as a legal no-op: the operand is still
    // evaluated (so e.g. an array-index bounds check still fires), but no runtime
    // deallocation call is emitted. This mirrors how `delete[] arr;` already tolerates
    // arrays of any element type. `delete` on a genuinely non-deletable receiver
    // (unknown/void expression, etc.) is caught earlier by expressionType() returning a
    // real value type here -- there is no way to reach this branch with something that
    // isn't a fully-typed value expression.
    const MethodCoreBinding* elementBinding = coreBinding(expression->first());
    const bool directElement = elementBinding != nullptr &&
        (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
         elementBinding->kind == MethodCoreOperationKind::RawElement);
    // EN: Freeze C-12: `delete items[i]` of a struct element with a destructor or owned
    //     fields drops that element in place, so a generic container's cleanup releases its
    //     value elements too; the slot is left zeroed. Other value types stay a no-op.
    // ES: Congelamiento C-12: `delete items[i]` de un elemento struct con destructor o campos
    //     owned destruye ese elemento en su lugar, asi la limpieza de un contenedor generico libera
    //     tambien sus elementos valor; el slot queda en cero. Los demas tipos valor siguen sin efecto.
    const bool dropsValueElement = !deletable && elementBinding != nullptr &&
        elementBinding->kind == MethodCoreOperationKind::ArrayElement &&
        memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(operandType.typeKey);
    // EN: Freeze (item 121): `delete p` of an owned parameter clears the parameter too.
    // ES: Congelamiento (elemento 121): `delete p` de un parametro owned tambien lo vacia.
    const bool clearsOwnedParameter = methodBodyModel_ != nullptr &&
        methodBodyModel_->deleteClearsOwnedParameter(expression);
    const bool clearsElementSlot = (directElement &&
        (isReferenceType(operandType.typeKey) || nativeString)) || dropsValueElement ||
        clearsOwnedParameter;
    auto operand = lowerExpression(
        expression->first(), memberOrdinal, clearsElementSlot, error);
    if (operand == nullptr) return nullptr;
    auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
    node->dynamicDestruction = isReferenceType(operandType.typeKey) && !nativeString;
    node->valueTypeNoOpDeletion = !deletable;
    if (!node->valueTypeNoOpDeletion)
    {
        node->objectLifetimeOwnership = node->dynamicDestruction
            ? sema::OwnershipKind::Owned : sema::OwnershipKind::None;
        node->storageOwnership = sema::OwnershipKind::Owned;
    }
    // Deleting a directly addressable reference element is an ownership operation on the
    // slot, not merely on a copied pointer.  Carry its address and type so LLVM can load
    // and clear it before destruction.  This prevents a later release-before-store from
    // observing a dangling owner and destroying the same allocation twice.
    node->booleanValue = clearsElementSlot;
    node->elementTypeKey = operandType.typeKey;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDeleteExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
    if (operandType == nullptr)
    {
        error = "Stage 41 cannot determine delete operand type";
        return nullptr;
    }
    // EN: A generic scalar delete whose T became a dynamic array releases it as `delete[]`.
    // ES: Un delete escalar generico cuyo T resulto arreglo dinamico lo libera como `delete[]`.
    const bool arrayForm = expression->flag() ||
        (methodBodyModel_ != nullptr && methodBodyModel_->scalarDeleteReleasesArray(expression));
    return arrayForm
        ? lowerArrayDeleteExpression(expression, memberOrdinal, *operandType, error)
        : lowerScalarDeleteExpression(expression, memberOrdinal, *operandType, error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDestroyExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
    if (operandType == nullptr)
    {
        error = "item 319 cannot determine destroy operand type";
        return nullptr;
    }
    auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
    if (operand == nullptr) return nullptr;
    bool dynamicDestruction = isReferenceType(operandType->typeKey);
    if (!dynamicDestruction && operandType->typeKey.ends_with('*'))
    {
        const std::string pointee = operandType->typeKey.substr(
            0, operandType->typeKey.size() - 1);
        dynamicDestruction = isReferenceType(pointee);
    }
    auto node = makeNode(
        ProjectOopHirNodeKind::ObjectDestruction, expression, "builtin:void");
    node->dynamicDestruction = dynamicDestruction;
    node->elementTypeKey = operandType->typeKey;
    node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
    node->storageOwnership = sema::OwnershipKind::Borrowed;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDestructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::DeleteExpression:
            return lowerDeleteExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::DestroyExpression:
            return lowerDestroyExpression(expression, memberOrdinal, error);
        default:
            error = "non-destruction expression reached HIR destruction lowering";
            return nullptr;
    }
}
}
