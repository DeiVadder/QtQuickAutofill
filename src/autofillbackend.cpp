// SPDX-License-Identifier: MIT
#include "autofillbackend.h"

AutofillBackend::~AutofillBackend() = default;

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
std::unique_ptr<AutofillBackend> AutofillBackend::createPlatformBackend()
{
    return std::make_unique<AutofillBackend>();
}
#endif
