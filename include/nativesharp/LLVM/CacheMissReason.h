#pragma once

namespace nativesharp::llvm_backend
{
// NSC305-P02. Stable reason for a per-code-unit artifact-cache lookup. Every miss must be able
// to explain its cause; the driver aggregates these reasons in the build report.
enum class CacheMissReason
{
    Hit,                   // Object restored and verified successfully.
    Disabled,              // Artifact cache is not enabled for this compilation.
    KeyMismatch,           // Cache key is not a canonical SHA-256.
    Cold,                  // No object is present for this key (first build or changed key).
    IncompletePublication, // Object present but its metadata was not fully published.
    Corruption,            // Metadata key or object digest mismatch; entry quarantined.
};
}
