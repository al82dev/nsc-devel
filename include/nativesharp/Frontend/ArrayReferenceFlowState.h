#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::frontend
{
// Experimental abstract facts only. This domain is not yet connected to
// expression analysis and cannot be used to accept or reject a program.
enum class ArrayReferenceLife { Alive, Dead, MaybeDead };
enum class ArrayReferenceRead { Alive, Dead, MaybeDead, Unknown };
// EN: Lifetime of the objects an array element use may name, from the life events alone (see
//     ArrayReferenceFlowState::assessUse). NoLifeEvent: no tracked object the element may name had
//     its life ended or put in doubt. Dead: the element definitely names one object that was
//     definitely destroyed. Unproven: some object it may name was destroyed, possibly destroyed,
//     or its owner was transferred without a proven outcome.
// ES: Vida de los objetos que puede nombrar el uso de un elemento, segun los eventos de vida solos
//     (ver ArrayReferenceFlowState::assessUse). NoLifeEvent: ningun objeto rastreado que el elemento
//     pueda nombrar termino ni puso en duda su vida. Dead: el elemento nombra con certeza un objeto
//     destruido con certeza. Unproven: algun objeto que puede nombrar se destruyo, quiza se
//     destruyo, o su dueno se transfirio sin resultado demostrado.
enum class ArrayReferenceUseLife { NoLifeEvent, Dead, Unproven };

// Observational provenance only. Completeness is never implied by an event list:
// implicit cleanup, exceptional edges and unmodelled expressions still need proof.
enum class ArrayReferenceTraceKind { Construction, Call, Write, Destruction, Branch };
struct ArrayReferenceTraceEvent final
{
    ArrayReferenceTraceKind kind = ArrayReferenceTraceKind::Call;
    const ast::AstNode* syntax = nullptr;
    std::optional<bool> branch;
    const ast::AstNode* source = nullptr;
    const ast::AstNode* destination = nullptr;
    std::optional<std::int64_t> index;
    bool operator==(const ArrayReferenceTraceEvent&) const = default;
};
struct ArrayReferencePathEvidence final
{
    std::vector<ArrayReferenceTraceEvent> events;
    bool complete = false;
};

struct ArrayReferenceSlot final
{
    std::size_t array = 0;
    std::optional<std::int64_t> index;

    auto operator<=>(const ArrayReferenceSlot&) const = default;
};

struct ArrayReferenceFlowState final
{
    using IdSet = std::set<std::size_t>;

    bool unknown = false;
    // Publication tracking is incomplete and is observational only.
    bool publicationUnknown = false;
    bool publicationCoverageUnknown = false;
    std::map<std::size_t, ArrayReferenceLife> objects;
    std::map<std::size_t, IdSet> owners;
    std::map<ArrayReferenceSlot, IdSet> slots;
    std::set<std::size_t> arrays;
    // Local aliases map directly to the allocation-site local that identifies
    // the array storage; slots and escape flags remain keyed by that root.
    std::map<std::size_t, std::size_t> arrayAliases;
    std::set<std::size_t> escapedArrays;
    std::set<std::size_t> unknownArrays;
    std::set<std::size_t> unknownObjects;
    std::set<std::size_t> exposedObjects;
    std::set<std::size_t> maybeAbsentOwners;
    std::set<ArrayReferenceSlot> maybeAbsentSlots;
    // Specialized parameter indices require the later reachability proof.
    std::set<ArrayReferenceSlot> lateValidationSlots;
    // These slots use an existential intraprocedural parameter value, not a
    // proven runtime argument or reachability from an entry point.
    std::set<ArrayReferenceSlot> potentialIndexSlots;
    // Only joins of two precise, reachable paths may establish these witnesses.
    std::map<ArrayReferenceSlot, std::size_t> deadPathWitnesses;
    std::vector<ArrayReferenceTraceEvent> traceEvents;
    bool traceUnknown = false;
    std::map<ArrayReferenceSlot, ArrayReferencePathEvidence> deadPathEvidence;
    // EN: Life events, kept apart from `objects` because markUnknown() degrades `objects` for
    //     reasons that do not end any life (unmodelled statements, loop entry). `lifeEnded` holds
    //     objects destroyed on every path; `lifeMaybeEnded` holds objects destroyed on some path or
    //     whose owner was transferred without a proven outcome. markUnknown() never touches them.
    // ES: Eventos de vida, separados de `objects` porque markUnknown() degrada `objects` por
    //     motivos que no terminan ninguna vida (sentencias no modeladas, entrada a un ciclo).
    //     `lifeEnded` guarda objetos destruidos en todo camino; `lifeMaybeEnded`, objetos
    //     destruidos en algun camino o cuyo dueno se transfirio sin resultado demostrado.
    //     markUnknown() nunca los toca.
    std::set<std::size_t> lifeEnded;
    std::set<std::size_t> lifeMaybeEnded;
    // EN: The path went through an operation that never completes normally (a call, constructor,
    //     destructor or delete without a normal exit). Uses on a stopped path are not reached and
    //     its life events never happen; joins ignore them.
    // ES: El camino paso por una operacion que nunca termina normalmente (una llamada, constructor,
    //     destructor o delete sin salida normal). Los usos en un camino detenido no se alcanzan y sus
    //     eventos de vida nunca ocurren; las uniones los ignoran.
    bool stopped = false;
    // EN: Objects an owner adopted from another owner through a proven `field = move(parameter)` of a
    //     receiver method; they live while the adopting owner lives and its field is not replaced.
    // ES: Objetos que un dueno adopto de otro mediante un `campo = move(parametro)` demostrado de un
    //     metodo receptor; viven mientras viva el dueno adoptante y no se reemplace su campo.
    std::map<std::size_t, IdSet> adopted;

    [[nodiscard]] std::optional<std::size_t> arrayForLocal(std::size_t local) const
    {
        if (arrays.contains(local)) return local;
        const auto found = arrayAliases.find(local);
        if (found == arrayAliases.end() || !arrays.contains(found->second)) return std::nullopt;
        return found->second;
    }

    void noteTrace(ArrayReferenceTraceKind kind, const ast::AstNode* syntax,
                   std::optional<bool> branch = std::nullopt,
                   const ast::AstNode* source = nullptr,
                   const ast::AstNode* destination = nullptr,
                   std::optional<std::int64_t> index = std::nullopt)
    {
        constexpr std::size_t limit = 256;
        if (traceUnknown) return;
        if (syntax == nullptr || traceEvents.size() >= limit)
        {
            traceEvents.clear();
            deadPathEvidence.clear();
            traceUnknown = true;
            return;
        }
        traceEvents.push_back({kind, syntax, branch, source, destination, index});
    }

    void create(std::size_t owner, std::size_t object)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        // Reusing an abstract allocation site can denote distinct dynamic
        // objects (notably across loop iterations), so do not claim precision.
        if (objects.contains(object) || owners.contains(owner))
        {
            // EN: The same id now names an older and a newer dynamic object.
            // ES: El mismo id nombra ahora un objeto dinamico anterior y uno nuevo.
            if (lifeEnded.erase(object) != 0) lifeMaybeEnded.insert(object);
            unknown = true;
            objects[object] = ArrayReferenceLife::MaybeDead;
            owners[owner].insert(object);
            return;
        }
        objects[object] = ArrayReferenceLife::Alive;
        owners[owner].insert(object);
    }

    void transfer(std::size_t from, std::size_t to)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        const auto found = owners.find(from);
        if (found == owners.end() || owners.contains(to))
        {
            markUnknown();
            return;
        }
        owners[to] = std::move(found->second);
        owners.erase(found);
        if (maybeAbsentOwners.erase(from) != 0) maybeAbsentOwners.insert(to);
    }

    void cleanup(std::size_t owner)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        const auto found = owners.find(owner);
        if (found == owners.end())
        {
            markUnknown();
            return;
        }
        // A joined owner can denote more than one object or be absent on one
        // path. Only a unique, definitely present owner proves destruction.
        const bool definite = found->second.size() == 1 && !maybeAbsentOwners.contains(owner);
        for (const std::size_t id : found->second)
        {
            objects[id] = definite ? ArrayReferenceLife::Dead : ArrayReferenceLife::MaybeDead;
            if (definite) unknownObjects.erase(id);
            if (definite && !lifeMaybeEnded.contains(id)) lifeEnded.insert(id);
            else lifeMaybeEnded.insert(id);
        }
        owners.erase(found);
        maybeAbsentOwners.erase(owner);
    }

    // EN: The owner gave its objects away with no proven outcome (an owned parameter whose effect is
    //     unknown, a callee that cannot be summarized). The objects may be destroyed or kept alive
    //     elsewhere: a `move` alone is not a destruction, but nothing proves the object is alive.
    //     An untracked owner degrades the state like before.
    // ES: El dueno entrego sus objetos sin resultado demostrado (un parametro owned de efecto
    //     desconocido, un callee sin resumen). Los objetos pueden destruirse o seguir vivos en otro
    //     lugar: un `move` solo no es una destruccion, pero nada demuestra que el objeto viva. Un
    //     dueno no rastreado degrada el estado como antes.
    void releaseUnproven(std::size_t owner)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        const auto found = owners.find(owner);
        if (found == owners.end())
        {
            markUnknown();
            return;
        }
        for (const std::size_t id : found->second)
        {
            if (objects[id] != ArrayReferenceLife::Dead) objects[id] = ArrayReferenceLife::MaybeDead;
            if (!lifeEnded.contains(id)) lifeMaybeEnded.insert(id);
        }
        owners.erase(found);
        maybeAbsentOwners.erase(owner);
    }

    void write(ArrayReferenceSlot slot, const IdSet& values, bool requiresLateValidation = false,
               bool potentialIndex = false)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        if (slot.index.has_value())
        {
            slots[slot] = values;
            maybeAbsentSlots.erase(slot);
            if (requiresLateValidation) lateValidationSlots.insert(slot);
            else lateValidationSlots.erase(slot);
            if (potentialIndex) potentialIndexSlots.insert(slot);
            else potentialIndexSlots.erase(slot);
        }
        else
        {
            slots[slot].insert(values.begin(), values.end());
            // An unknown index may have targeted another element.
            maybeAbsentSlots.insert(slot);
        }
    }

    void writeMaybe(ArrayReferenceSlot slot, const IdSet& values)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        const bool absent = !slots.contains(slot);
        slots[slot].insert(values.begin(), values.end());
        if (absent || !slot.index.has_value()) maybeAbsentSlots.insert(slot);
    }

    void markUnknown()
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        traceUnknown = true;
        unknown = true;
        for (auto& [id, life] : objects)
        {
            (void)id;
            if (life != ArrayReferenceLife::Dead) life = ArrayReferenceLife::MaybeDead;
        }
    }

    [[nodiscard]] ArrayReferenceRead read(ArrayReferenceSlot slot) const
    {
        // Without a proof covering every element, a dynamic index can name an
        // untracked slot and cannot establish the lifetime of its contents.
        if (!slot.index.has_value() || unknown || unknownArrays.contains(slot.array))
            return ArrayReferenceRead::Unknown;
        for (const ArrayReferenceSlot& candidate : maybeAbsentSlots)
            if (candidate.array == slot.array &&
                (!candidate.index.has_value() || !slot.index.has_value() ||
                 candidate.index == slot.index)) return ArrayReferenceRead::Unknown;
        IdSet values;
        for (const auto& [candidate, ids] : slots)
        {
            if (candidate.array != slot.array ||
                (candidate.index.has_value() && slot.index.has_value() &&
                 candidate.index != slot.index)) continue;
            values.insert(ids.begin(), ids.end());
        }
        if (values.empty()) return ArrayReferenceRead::Unknown;
        bool alive = false;
        bool dead = false;
        bool maybeDead = false;
        for (const std::size_t id : values)
        {
            const auto found = objects.find(id);
            if (found == objects.end()) return ArrayReferenceRead::Unknown;
            if (found->second != ArrayReferenceLife::Dead && unknownObjects.contains(id))
                return ArrayReferenceRead::Unknown;
            alive = alive || found->second == ArrayReferenceLife::Alive;
            dead = dead || found->second == ArrayReferenceLife::Dead;
            maybeDead = maybeDead || found->second == ArrayReferenceLife::MaybeDead;
        }
        if (maybeDead || (alive && dead)) return ArrayReferenceRead::MaybeDead;
        return dead ? ArrayReferenceRead::Dead : ArrayReferenceRead::Alive;
    }

    // EN: `to` adopts every object of `from` (a proven move into an owned field of `to`'s object).
    //     An untracked side degrades like releaseUnproven.
    // ES: `to` adopta todo objeto de `from` (un move demostrado a un campo owned del objeto de `to`).
    //     Un lado no rastreado degrada como releaseUnproven.
    void adopt(std::size_t from, std::size_t to)
    {
        deadPathWitnesses.clear();
        deadPathEvidence.clear();
        const auto source = owners.find(from);
        if (source == owners.end() || !owners.contains(to) || maybeAbsentOwners.contains(to))
        {
            releaseUnproven(from);
            return;
        }
        const IdSet ids = source->second;
        owners.erase(source);
        if (maybeAbsentOwners.erase(from) != 0) maybeAbsentOwners.insert(to);
        owners[to].insert(ids.begin(), ids.end());
        adopted[to].insert(ids.begin(), ids.end());
    }

    // EN: Something may have replaced or released what `owner` adopted (a call that receives it, a
    //     field write through it): those objects are no longer proven alive.
    // ES: Algo pudo reemplazar o liberar lo que `owner` adopto (una llamada que lo recibe, una
    //     escritura de campo a traves de el): esos objetos dejan de estar demostrados vivos.
    void releaseAdopted(std::size_t owner)
    {
        const auto found = adopted.find(owner);
        if (found == adopted.end()) return;
        for (const std::size_t id : found->second)
        {
            if (objects[id] != ArrayReferenceLife::Dead) objects[id] = ArrayReferenceLife::MaybeDead;
            if (!lifeEnded.contains(id)) lifeMaybeEnded.insert(id);
            if (const auto set = owners.find(owner); set != owners.end()) set->second.erase(id);
        }
        adopted.erase(found);
    }

    // EN: Lifetime of the objects a use of `slot` may name, from the life events alone. The
    //     candidates are every tracked object stored in the slot, in a slot of unknown index of the
    //     same array, in any slot of the array when the read index is unknown, and every object
    //     exposed to unmodelled code when the array also reached unmodelled code. Unlike read(),
    //     the global `unknown` flag does not hide a candidate: it widens the answer to Unproven
    //     when a candidate had a life event. Dead requires a definite slot whose only value was
    //     definitely destroyed and a state with no unknown writes.
    // ES: Vida de los objetos que puede nombrar un uso de `slot`, segun los eventos de vida. Son
    //     candidatos todo objeto rastreado guardado en la ranura, en una ranura de indice
    //     desconocido del mismo arreglo, en cualquier ranura si el indice leido es desconocido, y
    //     todo objeto expuesto a codigo no modelado cuando el arreglo tambien llego a ese codigo. A
    //     diferencia de read(), la bandera global `unknown` no oculta un candidato: si un candidato
    //     tuvo un evento de vida, la respuesta es Unproven. Dead exige una ranura definida cuyo unico
    //     valor se destruyo con certeza y un estado sin escrituras desconocidas.
    // Complexity: O(slots + exposed objects). Effects: none.
    [[nodiscard]] ArrayReferenceUseLife assessUse(ArrayReferenceSlot slot) const
    {
        if (stopped) return ArrayReferenceUseLife::NoLifeEvent;
        IdSet candidates;
        bool exact = slot.index.has_value() && !unknown && !unknownArrays.contains(slot.array) &&
            !escapedArrays.contains(slot.array) && !lateValidationSlots.contains(slot) &&
            !potentialIndexSlots.contains(slot);
        for (const auto& [candidate, ids] : slots)
        {
            if (candidate.array != slot.array) continue;
            const bool same = candidate.index.has_value() && slot.index.has_value() &&
                candidate.index == slot.index;
            if (!same && candidate.index.has_value() && slot.index.has_value()) continue;
            if (!same && !ids.empty()) exact = false;
            candidates.insert(ids.begin(), ids.end());
        }
        for (const ArrayReferenceSlot& absent : maybeAbsentSlots)
            if (absent.array == slot.array) exact = false;
        if (unknownArrays.contains(slot.array) || escapedArrays.contains(slot.array))
            candidates.insert(exposedObjects.begin(), exposedObjects.end());
        bool event = false;
        for (const std::size_t id : candidates)
            event = event || lifeEnded.contains(id) || lifeMaybeEnded.contains(id);
        if (!event) return ArrayReferenceUseLife::NoLifeEvent;
        if (exact && candidates.size() == 1 && lifeEnded.contains(*candidates.begin()))
            return ArrayReferenceUseLife::Dead;
        return ArrayReferenceUseLife::Unproven;
    }

    [[nodiscard]] bool hasDeadPathWitness(ArrayReferenceSlot slot) const
    {
        if (read(slot) != ArrayReferenceRead::MaybeDead || escapedArrays.contains(slot.array)) return false;
        const auto witness = deadPathWitnesses.find(slot);
        const auto contents = slots.find(slot);
        return witness != deadPathWitnesses.end() && contents != slots.end() &&
            contents->second.size() == 1 && contents->second.contains(witness->second);
    }

    [[nodiscard]] static ArrayReferenceFlowState join(
        const ArrayReferenceFlowState& left, const ArrayReferenceFlowState& right)
    {
        ArrayReferenceFlowState result = left;
        for (auto it = result.arrayAliases.begin(); it != result.arrayAliases.end();)
        {
            const auto other = right.arrayAliases.find(it->first);
            if (other == right.arrayAliases.end() || other->second != it->second)
                it = result.arrayAliases.erase(it);
            else ++it;
        }
        result.deadPathWitnesses.clear();
        result.deadPathEvidence.clear();
        for (const auto& [slot, ids] : left.slots)
        {
            const auto other = right.slots.find(slot);
            if (!slot.index.has_value() || ids.size() != 1 || other == right.slots.end() ||
                other->second != ids || left.escapedArrays.contains(slot.array) ||
                right.escapedArrays.contains(slot.array)) continue;
            const auto leftRead = left.read(slot);
            const auto rightRead = right.read(slot);
            if ((leftRead == ArrayReferenceRead::Dead && rightRead == ArrayReferenceRead::Alive) ||
                (leftRead == ArrayReferenceRead::Alive && rightRead == ArrayReferenceRead::Dead))
            {
                result.deadPathWitnesses.emplace(slot, *ids.begin());
                // A provenance witness is kept only for the same observed if
                // split with opposite branch choices and a shared prefix.
                const auto& dead = leftRead == ArrayReferenceRead::Dead ? left : right;
                const auto& live = leftRead == ArrayReferenceRead::Dead ? right : left;
                std::size_t prefix = 0;
                while (prefix < dead.traceEvents.size() && prefix < live.traceEvents.size() &&
                    dead.traceEvents[prefix] == live.traceEvents[prefix]) ++prefix;
                if (!dead.traceUnknown && !live.traceUnknown && prefix < dead.traceEvents.size() &&
                    prefix < live.traceEvents.size())
                {
                    const auto& a = dead.traceEvents[prefix];
                    const auto& b = live.traceEvents[prefix];
                    if (a.kind == ArrayReferenceTraceKind::Branch && b.kind == a.kind &&
                        a.syntax == b.syntax && a.branch.has_value() && b.branch.has_value() &&
                        a.branch != b.branch)
                        result.deadPathEvidence.emplace(slot, ArrayReferencePathEvidence{dead.traceEvents, false});
                }
            }
        }
        // Once branches differ there is no single complete linear trace.
        if (left.traceUnknown || right.traceUnknown || left.traceEvents != right.traceEvents)
        {
            result.traceEvents.clear();
            result.traceUnknown = true;
        }
        // EN: A stopped path never reaches the join: the other path's life events are the result.
        // ES: Un camino detenido nunca llega a la union: el resultado son los eventos del otro camino.
        result.stopped = left.stopped && right.stopped;
        if (left.stopped != right.stopped)
        {
            const ArrayReferenceFlowState& live = left.stopped ? right : left;
            result.lifeEnded = live.lifeEnded;
            result.lifeMaybeEnded = live.lifeMaybeEnded;
        }
        else
        {
            // EN: Ended on both paths stays ended; ended on one path only is a possible end.
            // ES: Terminado en ambos caminos sigue terminado; en uno solo es un fin posible.
            result.lifeEnded.clear();
            result.lifeMaybeEnded = left.lifeMaybeEnded;
            result.lifeMaybeEnded.insert(right.lifeMaybeEnded.begin(), right.lifeMaybeEnded.end());
            for (const std::size_t id : left.lifeEnded)
            {
                if (right.lifeEnded.contains(id) && !result.lifeMaybeEnded.contains(id))
                    result.lifeEnded.insert(id);
                else result.lifeMaybeEnded.insert(id);
            }
            for (const std::size_t id : right.lifeEnded)
                if (!result.lifeEnded.contains(id)) result.lifeMaybeEnded.insert(id);
        }
        for (const auto& [owner, ids] : right.adopted) result.adopted[owner].insert(ids.begin(), ids.end());
        result.unknown = left.unknown || right.unknown;
        result.publicationUnknown = left.publicationUnknown || right.publicationUnknown;
        result.publicationCoverageUnknown =
            left.publicationCoverageUnknown || right.publicationCoverageUnknown;
        result.arrays.insert(right.arrays.begin(), right.arrays.end());
        result.escapedArrays.insert(right.escapedArrays.begin(), right.escapedArrays.end());
        result.unknownArrays.insert(right.unknownArrays.begin(), right.unknownArrays.end());
        result.unknownObjects.insert(right.unknownObjects.begin(), right.unknownObjects.end());
        result.exposedObjects.insert(right.exposedObjects.begin(), right.exposedObjects.end());
        for (const auto& [id, life] : right.objects)
        {
            const auto found = result.objects.find(id);
            if (found == result.objects.end()) result.objects[id] = ArrayReferenceLife::MaybeDead;
            else if (found->second != life) found->second = ArrayReferenceLife::MaybeDead;
        }
        for (auto& [id, life] : result.objects)
            if (!right.objects.contains(id)) life = ArrayReferenceLife::MaybeDead;
        result.maybeAbsentOwners.insert(right.maybeAbsentOwners.begin(), right.maybeAbsentOwners.end());
        for (const auto& [owner, ids] : left.owners)
            if (!right.owners.contains(owner)) result.maybeAbsentOwners.insert(owner);
        for (const auto& [owner, ids] : right.owners)
            if (!left.owners.contains(owner)) result.maybeAbsentOwners.insert(owner);
        result.maybeAbsentSlots.insert(right.maybeAbsentSlots.begin(), right.maybeAbsentSlots.end());
        result.lateValidationSlots.insert(right.lateValidationSlots.begin(), right.lateValidationSlots.end());
        result.potentialIndexSlots.insert(right.potentialIndexSlots.begin(), right.potentialIndexSlots.end());
        for (const auto& [slot, ids] : left.slots)
            if (!right.slots.contains(slot)) result.maybeAbsentSlots.insert(slot);
        for (const auto& [slot, ids] : right.slots)
            if (!left.slots.contains(slot)) result.maybeAbsentSlots.insert(slot);
        for (const auto& [owner, ids] : right.owners)
            result.owners[owner].insert(ids.begin(), ids.end());
        for (const auto& [slot, ids] : right.slots)
            result.slots[slot].insert(ids.begin(), ids.end());
        return result;
    }
};
}
