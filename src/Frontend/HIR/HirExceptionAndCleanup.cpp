#include "HirLoweringSupport.h"

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

// EN: Freeze 3.0 C-11 (found while probing value lifetimes): the result of a call that hands its
//     caller a value to destroy (an owned object or string, or a struct with a destructor or
//     owned fields) leaked when it was discarded (`Make();`) or only used as a receiver
//     (`Make().Count`, `Make().Id()`). Such a temporary now gets a hidden owner that the full
//     expression destroys. A receiver is owned only when its consumer yields a plain scalar,
//     because a reference or a string taken from the temporary could outlive it.
// ES: Congelamiento 3.0 C-11 (hallazgo al sondear lifetimes de valores): el resultado de una
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

// EN: Freeze 3.0 (plan item 131a): helpers of ownReceiversUntilFullExpression. A statement node ends a full
//     expression; so does a node that evaluates an operand only on some paths or suspends, because a cleanup
//     closed past it would destroy a temporary that one path never made (or keep it across a suspension).
// ES: Congelamiento 3.0 (elemento 131a del plan): auxiliares de ownReceiversUntilFullExpression. Un nodo de
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

// EN: Freeze 3.0 (plan item 135): `borrowed` is a value operand of a `?:` (not its condition) or an operand of a
//     `??`, so the borrow reaches the result on some path.
// ES: Congelamiento 3.0 (elemento 135 del plan): `borrowed` es un operando de valor de un `?:` (no su condicion)
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

// EN: Freeze 3.0 (plan item 135a): `argument` is an owned temporary lent to a parameter that does not take
//     ownership (its hidden owner destroys it at the end of the full expression).
// ES: Congelamiento 3.0 (elemento 135a del plan): `argument` es un temporal owned prestado a un parametro que no
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

bool ProjectOopHirModel::isFreshDroppableTemporary(const ProjectOopHirNode* node) const
{
    if (node == nullptr || node->referenceResult || memberModel_ == nullptr) return false;
    const bool producer = node->kind == ProjectOopHirNodeKind::InstanceCall ||
        node->kind == ProjectOopHirNodeKind::StaticCall ||
        node->kind == ProjectOopHirNodeKind::DelegateInvoke ||
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
    // EN: Freeze 3.0 (receiver temporaries): a consumer whose result is plain data (a struct of
    //     scalars, say) copies it out of the temporary, so the temporary is owned and destroyed
    //     too; before, only a scalar result was, and `Make().Inner` leaked the temporary.
    // ES: Congelamiento 3.0 (temporales receptores): un consumidor cuyo resultado son datos
    //     simples (un struct de escalares, por ejemplo) los copia fuera del temporal, asi que el
    //     temporal tambien se posee y se destruye; antes solo con un resultado escalar, y
    //     `Make().Inner` perdia el temporal.
    const bool plainDataResult = !consumer->referenceResult && memberModel_ != nullptr &&
        memberModel_->typeIsPlainData(consumer->typeKey);
    if (!consumesReceiver(*consumer) || (!hasPlainResult(*consumer) && !plainDataResult)) return false;
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
                isFreshDroppableTemporary(inner->get()))
            {
                ProjectOopHirNode* producer = inner->get();
                *inner = ownTemporary(std::move(*inner));
                pending.push_back(producer);
                continue;
            }
            if (node->kind == ProjectOopHirNodeKind::ExpressionStatement &&
                isFreshDroppableTemporary(child.get()))
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
}

// EN: Freeze 3.0 (plan item 135a). A call that lends an owned temporary argument (`Id(MakeText())`) came wrapped
//     in its own FullExpressionCleanup, so the temporary died right after the call even when the call returned it
//     and an enclosing expression still read it (`Id(MakeText()).Length`). When the result is a non-owned
//     reference, the wrapper is removed and the full expression is closed instead, as for a receiver temporary.
//     Storing that result is rejected by the analysis, so the borrow never outlives the expression.
// ES: Congelamiento 3.0 (elemento 135a del plan). Una llamada que presta un temporal owned argumento
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

// EN: Freeze 3.0 (plan item 131a). Three receiver temporaries leaked: a field read that is not plain data
//     (`Make().Name.Length`), a call that returns a new owner (`Make().Next().V`) and a field store target
//     (`Make().V = 12;`). Each one now gets a hidden owner that the full expression destroys, after every other
//     use and in reverse order of creation. A borrowed read is owned only when its value ends in a plain value
//     (or is discarded): storing it is rejected by the analysis, and any other flow keeps the old behavior, so
//     no reference outlives the temporary.
// ES: Congelamiento 3.0 (elemento 131a del plan). Tres temporales receptores se fugaban: una lectura de campo que
//     no son datos simples (`Make().Name.Length`), una llamada que devuelve un owner nuevo (`Make().Next().V`) y
//     el destino de una escritura de campo (`Make().V = 12;`). Cada uno recibe un dueno oculto que la expresion
//     completa destruye, despues de todo otro uso y en orden inverso de creacion. Una lectura prestada se posee
//     solo cuando su valor termina en un valor simple (o se descarta): guardarla lo rechaza el analisis, y otro
//     flujo conserva la conducta anterior, asi que ninguna referencia sobrevive al temporal.
//
// EN: Freeze 3.0 (plan item 135). The same hidden owner now covers every form the analysis tracks with the
//     temporary's provenance: a borrow that goes on through a non-owned call or getter result, a `?:` or a `??`,
//     and `(new T()).Field`. A borrow that crosses a `?:` or `??` comes from a temporary made on one path only, so
//     its full expression clears the owner slot when it starts and the cleanup skips a temporary never made.
//     That needs a pointer owner and no suspension inside the full expression; otherwise the old behavior stays.
// ES: Congelamiento 3.0 (elemento 135 del plan). El mismo dueno oculto cubre ahora toda forma que el analisis
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
// EN: Freeze 3.0 (plan item 135): a `new` of a class is a temporary like an owned-returning call; an instance call
//     on one is rejected by the analysis (131b) and never reaches here. A non-owned call result follows the same
//     borrow path as a field read; `crossesBranch` reports a `?:` or `??` on that path.
// ES: Congelamiento 3.0 (elemento 135 del plan): un `new` de una clase es un temporal como una llamada con
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
// EN: Freeze 3.0 (plan item 135): the walk goes on through every node the analysis carries the provenance
//     through (passesBorrowThrough, `?:`, `??`) and also ends at a field store through the borrow or at a node
//     that makes a new owner from it (endsInNewOwner).
// ES: Congelamiento 3.0 (elemento 135 del plan): el recorrido sigue por todo nodo por el que el analisis lleva la
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

// EN: Freeze 3.0 (plan item 135): `new T(...)` of a class whose object and storage this expression owns (not a
//     placement or stack-promoted construction).
// ES: Congelamiento 3.0 (elemento 135 del plan): `new T(...)` de una clase cuyo objeto y almacenamiento posee
//     esta expresion (no una construccion en sitio ni promovida a la pila).
bool ProjectOopHirModel::isOwnedNewObject(const ProjectOopHirNode& node) const
{
    return node.kind == ProjectOopHirNodeKind::ObjectConstruction &&
        node.objectLifetimeOwnership == sema::OwnershipKind::Owned &&
        node.storageOwnership == sema::OwnershipKind::Owned && !node.placementConstructed &&
        !node.stackPromotedObject && isReferenceType(node.typeKey);
}

// EN: Freeze 3.0 (plan item 135): `consumer` makes a new owner that cannot keep `borrowed`: a new string (a
//     concatenation or any string-returning call), or an object returned by a method called on the borrow.
//     An object built from the borrow as an argument may keep it, so that one does not end the borrow.
// ES: Congelamiento 3.0 (elemento 135 del plan): `consumer` crea un owner nuevo que no puede guardar `borrowed`:
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

// EN: Freeze 3.0 (plan item 135): the result of `consumer` is still a borrow of the temporary: a further
//     reference field read, a call or getter whose result is a non-owned reference, or a conversion.
// ES: Congelamiento 3.0 (elemento 135 del plan): el resultado de `consumer` sigue siendo un prestamo del
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
// EN: Freeze 3.0 (plan item 135): the choice is made before the receiver gets its owner
//     (planFullExpressionClose): `root` is the node to wrap, or `enclosing` the cleanup that already closes the
//     expression. A borrow that crosses a branch needs an enclosing cleanup without a suspension, because that
//     cleanup clears the owner slots when it starts (booleanValue).
// ES: Congelamiento 3.0 (elemento 135 del plan): la eleccion se hace antes de dar dueno al receptor
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
    // EN: Freeze 3.0 (owned arrays): an owned array field is addressed too, so it is cleared.
    // ES: Congelamiento 3.0 (arreglos owned): un campo de arreglo owned tambien se direcciona,
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
    // EN: Freeze 3.0 (item 130, F2): a plain local array does not own its references.
    // ES: Congelamiento 3.0 (elemento 130, F2): un local arreglo plano no posee sus referencias.
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
    // EN: Freeze 3.0 C-12: `delete items[i]` of a struct element with a destructor or owned
    //     fields drops that element in place, so a generic container's cleanup releases its
    //     value elements too; the slot is left zeroed. Other value types stay a no-op.
    // ES: Congelamiento 3.0 C-12: `delete items[i]` de un elemento struct con destructor o campos
    //     owned destruye ese elemento en su lugar, asi la limpieza de un contenedor generico libera
    //     tambien sus elementos valor; el slot queda en cero. Los demas tipos valor siguen sin efecto.
    const bool dropsValueElement = !deletable && elementBinding != nullptr &&
        elementBinding->kind == MethodCoreOperationKind::ArrayElement &&
        memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(operandType.typeKey);
    // EN: Freeze 3.0 (item 121): `delete p` of an owned parameter clears the parameter too.
    // ES: Congelamiento 3.0 (elemento 121): `delete p` de un parametro owned tambien lo vacia.
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
    return expression->flag()
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
