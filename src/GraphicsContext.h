#pragma once

#include "IRenderer.h"
#include "IGUI.h"

class GraphicsContext {
public:
    static IRenderer* renderer;
    static IGUI* gui;
};