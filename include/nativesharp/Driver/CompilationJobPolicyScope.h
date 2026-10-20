#pragma once

namespace nativesharp::driver
{
class CompilationJobPolicy;

class CompilationJobPolicyScope final
{
  public:
    explicit CompilationJobPolicyScope(const CompilationJobPolicy &policy) noexcept;
    ~CompilationJobPolicyScope();

    CompilationJobPolicyScope(const CompilationJobPolicyScope &) = delete;
    CompilationJobPolicyScope &operator=(const CompilationJobPolicyScope &) = delete;

  private:
    const CompilationJobPolicy *previous_;
};
} // namespace nativesharp::driver
