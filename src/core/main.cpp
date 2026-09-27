#include "core/Settings.h"
#include "editor/Editor.h"
#include "graphics/Renderer.h"
#include "io/FileParser.h"
#include "io/PathUtils.h"
#include "ui/ImguiInterface.h"
#include "utils/Log.h"
#include "window/WindowSystem.h"

#include <charconv>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#endif

namespace
{
std::vector<std::string> ReadArguments([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    std::vector<std::string> arguments;
#ifdef _WIN32
    int count = 0;
    auto* raw = CommandLineToArgvW(GetCommandLineW(), &count);
    if(!raw) {
        throw std::runtime_error("Cannot read the native command line");
    }
    const std::unique_ptr<wchar_t*, decltype(&LocalFree)> wideArguments(raw, &LocalFree);
    for(int index = 0; index < count; ++index) {
        arguments.push_back(PathToUtf8(std::filesystem::path(wideArguments.get()[index])));
    }
#else
    for(int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
#endif
    return arguments;
}

std::filesystem::path ExecutableDirectory(
    [[maybe_unused]] const std::filesystem::path& executable) {
#ifdef _WIN32
    std::vector<wchar_t> path(260);
    while(path.size() <= 32768) {
        const DWORD length =
            GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if(length == 0) {
            throw std::runtime_error("Cannot determine the executable directory");
        }
        if(length < path.size()) {
            return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
        }
        path.resize(path.size() * 2);
    }
    throw std::runtime_error("Executable path exceeds the supported Windows path length");
#else
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if(!error) {
        return path.parent_path();
    }
    return std::filesystem::absolute(executable).parent_path();
#endif
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = ReadArguments(argc, argv);
        const auto executableDirectory = ExecutableDirectory(PathFromUtf8(arguments.at(0)));
        auto assetRoot = executableDirectory / "res";
        auto sceneFile = executableDirectory / "model_config.json";
        RenderSettings settings;
        std::size_t frameLimit = 0;
        std::vector<std::filesystem::path> initialModels;
        for(std::size_t index = 1; index < arguments.size(); ++index) {
            const std::string_view option(arguments[index]);
            const auto value = [&]() -> std::string_view {
                if(index + 1 >= arguments.size()) {
                    throw std::invalid_argument("Missing value for " + std::string(option));
                }
                return arguments[++index];
            };
            if(option == "--assets") {
                assetRoot = std::filesystem::absolute(PathFromUtf8(value()));
            }
            else if(option == "--scene") {
                sceneFile = std::filesystem::absolute(PathFromUtf8(value()));
            }
            else if(option == "--load") {
                initialModels.emplace_back(std::filesystem::absolute(PathFromUtf8(value())));
            }
            else if(option == "--no-vsync") {
                settings.vsync = false;
            }
            else if(option == "--hidden") {
                settings.visible = false;
            }
            else if(option == "--frames") {
                const auto text = value();
                const auto [end, error] =
                    std::from_chars(text.data(), text.data() + text.size(), frameLimit);
                if(error != std::errc{} || end != text.data() + text.size() || frameLimit == 0) {
                    throw std::invalid_argument("--frames requires a positive integer");
                }
            }
            else if(option == "--help") {
                Log(LogLevel::Info,
                    "MedievalPort [--assets directory] [--scene file] [--load model.obj] "
                    "[--no-vsync] [--hidden] [--frames count]");
                return 0;
            }
            else {
                throw std::invalid_argument("Unknown option: " + std::string(option));
            }
        }
        if(!std::filesystem::is_directory(assetRoot / "shaders")) {
            throw std::runtime_error("Shader assets not found under " + PathToUtf8(assetRoot) +
                                     "; specify --assets or deploy res beside the executable");
        }

        // Reverse destruction: editor -> renderer -> joined loader/scene -> UI -> GL context.
        WindowSystem window(settings);
        ImguiInterface gui(window.GetWindow());
        FileParser assets(window.GetContext(), sceneFile);
        assets.LoadSavedFiles();
        for(const auto& model : initialModels) {
            assets.LoadAsset(model);
        }
        Renderer renderer(window.GetContext(), assetRoot, settings);
        Editor editor(window, renderer, gui, assets);
        editor.Run(frameLimit);
        return 0;
    } catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
        return 1;
    }
}
