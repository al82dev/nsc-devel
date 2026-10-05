#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    ThreadSafetyTypeFacts composite;
    composite.typeName = "WorkPacket";
    composite.members = {
        {"id", ThreadSafetyMemberKind::Value, true, true},
        {"counter", ThreadSafetyMemberKind::Atomic, false, false}};
    const ThreadSafetyDecision compositeDecision = evaluateThreadSafety(composite);
    tests.expect(compositeDecision.send && compositeDecision.sync,
        "composites derive Send and Sync structurally and atomics provide both guarantees");

    ThreadSafetyTypeFacts shared = composite;
    shared.typeName = "SharedView";
    shared.members = {{"view", ThreadSafetyMemberKind::SharedReference, true, false}};
    const ThreadSafetyDecision sharedDecision = evaluateThreadSafety(shared);
    tests.expect(!sharedDecision.send && !sharedDecision.sync,
        "a shared reference requires the referred type to be Sync");

    ThreadSafetyTypeFacts raw = composite;
    raw.typeName = "NativeHandle";
    raw.members = {{"handle", ThreadSafetyMemberKind::RawPointer, true, true}};
    const ThreadSafetyDecision rawDecision = evaluateThreadSafety(raw);
    tests.expect(!rawDecision.send && !rawDecision.sync,
        "raw pointers never receive automatic Send or Sync guarantees");

    ThreadSafetyTypeFacts dynamic = composite;
    dynamic.typeName = "DynamicJob";
    dynamic.members = {{"job", ThreadSafetyMemberKind::DynamicTrait,
        true, true, true, false}};
    const ThreadSafetyDecision dynamicDecision = evaluateThreadSafety(dynamic);
    tests.expect(dynamicDecision.send && !dynamicDecision.sync,
        "dynamic traits carry explicit Send and Sync bounds");

    ThreadSafetyTypeFacts optedOut = composite;
    optedOut.typeName = "ThreadBound";
    optedOut.negativeSendImpl = true;
    const ThreadSafetyDecision optedOutDecision = evaluateThreadSafety(optedOut);
    tests.expect(!optedOutDecision.send && optedOutDecision.sync,
        "a negative implementation overrides otherwise structural derivation");

    raw.unsafeSendImpl = true;
    const ThreadSafetyDecision unaudited = evaluateThreadSafety(raw);
    raw.unsafeProofReviewed = true;
    const ThreadSafetyDecision audited = evaluateThreadSafety(raw);
    tests.expect(!unaudited.send && unaudited.reason.find("audited safety proof") != std::string::npos &&
            audited.send && audited.usedUnsafeSend && !audited.sync,
        "unsafe impl can override one guarantee only after an audited proof");

    ThreadSafetyTypeFacts closure = composite;
    closure.typeName = "async closure#1";
    closure.asyncClosure = true;
    closure.members = {{"captured_ui", ThreadSafetyMemberKind::Value, false, false}};
    const ThreadSafetyDecision closureDecision = evaluateThreadSafety(closure);
    tests.expect(!closureDecision.send &&
            closureDecision.reason.find("async closure captures") != std::string::npos,
        "async closures derive guarantees from their captured environment");

    const ThreadSafetyMetadata metadata = exportThreadSafetyMetadata(
        composite.typeName, compositeDecision);
    const auto imported = importThreadSafetyMetadata(metadata, error);
    tests.expect(imported.has_value() && imported->send && imported->sync,
        "DLL metadata preserves structural Send and Sync decisions");
    ThreadSafetyMetadata incompatible = metadata;
    ++incompatible.contractMajor;
    tests.expect(!importThreadSafetyMetadata(incompatible, error).has_value() &&
            error.find("major version") != std::string::npos,
        "DLL metadata fails closed across an incompatible concurrency ABI");
    return tests.result();
}
