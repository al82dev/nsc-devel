#pragma once
namespace nativesharp::frontend
{
// Analysis context only; never evidence that this syntax executes.
enum class MethodNameBindingContext { Unknown, TopLevel, Lambda };
}
