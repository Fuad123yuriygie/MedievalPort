#include "WindowSystem.h"
#include "GraphicsContext.h"
#include "Renderer.h"
#include "ImguiInterface.h"
#include "Editor.h"

int main() {
    WindowSystem::GetInstance(); // init window
    GraphicsContext::renderer = &Renderer::GetInstance();
    GraphicsContext::gui = new ImguiInterface();
    GraphicsContext::gui->Init(WindowSystem::GetInstance().GetWindow());
    Editor editor;
    editor.Run();
    delete GraphicsContext::gui;
    return 0;
}
