#include "platform/public/pipe.h"

#include "platform/api/condition_variable.h"
#include "platform/api/mutex.h"
#include "platform/api/platform.h"

namespace location {
namespace nearby {

namespace {
using Platform = api::ImplementationPlatform;
}

Pipe::Pipe() {
  auto mutex = Platform::CreateMutex(api::Mutex::Mode::kRegular);
  auto cond = Platform::CreateConditionVariable(mutex.get());
  Setup(std::move(mutex), std::move(cond));
}

}  // namespace nearby
}  // namespace location
