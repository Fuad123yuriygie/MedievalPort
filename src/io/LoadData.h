#pragma once

#include "core/Settings.h"
#include "io/SceneTypes.h"

#include <stop_token>

// Import work for one scene instance. Runs on the loader worker; the returned structure owns all
// CPU data needed to create GPU resources on the context thread.
PendingModelData LoadModelData(ModelDescription description, const ImportSettings& settings = {},
                               std::stop_token cancellation = {});
