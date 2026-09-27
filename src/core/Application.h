#pragma once

#include "core/Camera.h"
#include "core/Control.h"

#include <cstddef>

class WindowSystem;
class Renderer;
class ImguiInterface;
class FileParser;

class Application {
public:
    Application(WindowSystem& window, Renderer& renderer, ImguiInterface& gui, FileParser& assets,
                const CameraSettings& cameraSettings = {});
    virtual ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run(std::size_t frameLimit = 0);

protected:
    virtual void UpdateGUI() = 0;
    FileParser& Assets() const {
        return assets;
    }

private:
    void Update(float deltaSeconds);
    void RenderScene(int width, int height);

    WindowSystem& window;
    Renderer& renderer;
    ImguiInterface& gui;
    FileParser& assets;
    Camera camera;
    Control control;
};
