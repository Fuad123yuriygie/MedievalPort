#pragma once

class GraphicsContext;

// Installs the OpenGL debug callback. synchronous selects debug-output delivery: it is intended for
// development/troubleshooting, because synchronous delivery makes the triggering call wait for the
// callback. The callback filters messages before constructing any strings.
void InstallDebugOutput(const GraphicsContext& context, bool synchronous);
