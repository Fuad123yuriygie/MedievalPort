#include "core/Application.h"

#include "graphics/Renderer.h"
#include "io/FileParser.h"
#include "ui/ImguiInterface.h"
#include "window/WindowSystem.h"

#include <GLFW/glfw3.h>
#include <algorithm>

Application::Application(WindowSystem& window, Renderer& renderer, ImguiInterface& gui,
                         FileParser& assets, const CameraSettings& cameraSettings)
    : window(window), renderer(renderer), gui(gui), assets(assets), camera(cameraSettings),
      control(window.GetWindow()) {
}

void Application::Run(std::size_t frameLimit) {
    double lastFrameTime = glfwGetTime();
    std::size_t frameCount = 0;
    while(!window.ShouldClose() && (frameLimit == 0 || frameCount < frameLimit)) {
        window.PollEvents();
        const double now = glfwGetTime();
        const float deltaSeconds = static_cast<float>(
            std::clamp(now - lastFrameTime, 0.0, double{RenderSettings::maxDeltaSeconds}));
        lastFrameTime = now;
        const auto [width, height] = window.GetFramebufferSize();
        camera.Resize(width, height);

        // Events -> CPU/GPU handoff -> UI/input -> scene -> skybox -> UI -> present.
        gui.NewFrame();
        Update(deltaSeconds);
        UpdateGUI();
        if(width > 0 && height > 0) {
            RenderScene(width, height);
            renderer.BeginUI();
        }
        gui.Render();
        if(width > 0 && height > 0) {
            window.Present();
        }
        else {
            window.WaitForEvents(RenderSettings::maxDeltaSeconds);
        }
        ++frameCount;
    }
}

void Application::Update(float deltaSeconds) {
    for(const auto& path : window.TakeDroppedFiles()) {
        assets.LoadAsset(path);
    }
    assets.ProcessPendingModels();
    control.Update(camera, deltaSeconds, gui.WantsMouse(), gui.WantsKeyboard());
}

void Application::RenderScene(int width, int height) {
    const auto draws = assets.BuildDrawList(camera.GetProjection() * camera.GetView());
    renderer.BeginScene(width, height);
    renderer.DrawScene(draws);
    renderer.DrawSkybox(camera.GetView(), camera.GetProjection());
}
