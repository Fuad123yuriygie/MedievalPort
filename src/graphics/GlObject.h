#pragma once

class GraphicsContext;

class GlObject {
public:
    enum class Kind {
        Buffer,
        VertexArray,
        Texture,
        Program
    };

    GlObject(const GraphicsContext& context, Kind kind, unsigned textureTarget = 0);
    ~GlObject();
    GlObject(const GlObject&) = delete;
    GlObject& operator=(const GlObject&) = delete;
    GlObject(GlObject&& other) noexcept;
    GlObject& operator=(GlObject&& other) noexcept;

    unsigned GetId() const {
        return id;
    }
    void RequireCurrent() const;

private:
    void Reset() noexcept;

    const GraphicsContext* context;
    Kind kind;
    unsigned id = 0;
};
