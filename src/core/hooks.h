#pragma once

#include "pch.h"

namespace core::Hooks {

bool install();
void uninstall();
void requestUnload();
bool isUnloadRequested();

} // namespace core::Hooks
