#include "CppClassApi.h"
namespace Demo {
static Widget instance;
static int numberStorage = 73;
int* Widget::sharedRef = &numberStorage;
const int* Widget::sharedView = &numberStorage;
int* const Widget::sharedFixed = &numberStorage;
static Hidden hiddenStorage{31};
static Payload payloadStorage{11, &numberStorage, &numberStorage, &numberStorage, nullptr, &payloadStorage, &hiddenStorage};
Hidden* Widget::sharedHidden = &hiddenStorage;
Payload* Widget::sharedPayload = &payloadStorage;
const Payload* Widget::sharedViewPayload = &payloadStorage;
int Payload::select() { return 3; }
int Payload::select() const { return 7; }
void Payload::mutateOnly() { ++value; }
int Payload::mix(int*) { return 1; }
int Payload::mix(const int*) { return 2; }
int Payload::mix(int*) const { return 4; }
int Payload::mix(const int*) const { return 8; }
int Payload::cross(int*) const { return 16; }
int Payload::cross(const int*) { return 32; }
Widget* make() noexcept { return &instance; }
int* number() noexcept { return &numberStorage; }
Payload* Widget::getPayload() const { return &payloadStorage; }
const Payload* Widget::viewPayload(const Payload* value) const { return value; }
const Payload* Widget::viewPayloadStatic(const Payload* value) { return value; }
int* Widget::forward(int* value) const { *value += 3; return value; }
const int* Widget::view(const int* value) const { return value; }
const int* Widget::viewStatic(const int* value) { return value; }
int Widget::classify(int*) const { return 2; }
int Widget::classify(const int*) const { return 3; }
int Widget::classifyStatic(int*) { return 5; }
int Widget::classifyStatic(const int*) { return 7; }
int* Widget::forwardStatic(int* value) { *value += 7; return value; }
Payload* Widget::forwardPayload(Payload* value) const { return value; }
Payload* Widget::forwardPayloadStatic(Payload* value) { return value; }
}
