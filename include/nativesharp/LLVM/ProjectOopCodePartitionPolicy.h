#pragma once

namespace nativesharp::llvm_backend
{
enum class ProjectOopCodePartitionPolicy
{
    Balanced,
    StableIncremental,
    CacheAwareBalanced,
    /// EN: Pure function of the MIR: each function goes to the bucket of the FNV-1a hash of its
    ///     mangled name, so adding, removing or editing one function never moves another one and
    ///     an incremental build partitions exactly like a clean build. When a bucket would be
    ///     empty (small programs), the whole partition falls back to Balanced, which is also a
    ///     pure function of the MIR, so the unit count is always min(requested, candidates).
    /// ES: Funcion pura del MIR: cada funcion va a la cubeta del hash FNV-1a de su nombre
    ///     decorado, asi que agregar, quitar o editar una funcion nunca mueve otra y un build
    ///     incremental parte igual que un build limpio. Si alguna cubeta quedaria vacia
    ///     (programas chicos), toda la particion usa Balanced, que tambien es funcion pura del
    ///     MIR, asi que el numero de unidades siempre es min(solicitadas, candidatas).
    DeterministicIncremental
};
}
