#pragma once
#include <memory>

#include "config.h"
#include "LLMProvider.h"

namespace quantclaw::providers {

std::unique_ptr<LLMProvider> CreateProvider(const Config& cfg);

}
