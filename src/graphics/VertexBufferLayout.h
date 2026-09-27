#pragma once

#include <algorithm>
#include <glad/glad.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

struct VertexBufferElement {
    unsigned location;
    unsigned type;
    unsigned count;
    unsigned char normalized;
    unsigned offset;

    static constexpr unsigned GetSizeOfType(unsigned type) {
        switch(type) {
        case GL_FLOAT:
            return sizeof(float);
        case GL_INT:
            return sizeof(int);
        case GL_UNSIGNED_INT:
            return sizeof(unsigned);
        case GL_UNSIGNED_BYTE:
            return sizeof(unsigned char);
        default:
            throw std::invalid_argument("Unsupported vertex element type");
        }
    }
};

class VertexBufferLayout {
public:
    template <typename T> void Push(unsigned location, unsigned count) {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, unsigned> ||
                          std::is_same_v<T, int> || std::is_same_v<T, unsigned char>,
                      "Unsupported vertex attribute type");
        if(count == 0 || count > 4 ||
           std::any_of(elements.begin(), elements.end(), [location](const auto& element) {
               return element.location == location;
           })) {
            throw std::invalid_argument("Invalid vertex attribute count or duplicate location");
        }
        constexpr unsigned type = std::is_same_v<T, float>      ? GL_FLOAT
                                  : std::is_same_v<T, unsigned> ? GL_UNSIGNED_INT
                                  : std::is_same_v<T, int>      ? GL_INT
                                                                : GL_UNSIGNED_BYTE;
        constexpr unsigned char normalized = std::is_same_v<T, unsigned char> ? GL_TRUE : GL_FALSE;
        const unsigned size = count * VertexBufferElement::GetSizeOfType(type);
        if(stride > std::numeric_limits<unsigned>::max() - size) {
            throw std::overflow_error("Vertex stride overflow");
        }
        elements.push_back({location, type, count, normalized, stride});
        stride += size;
    }

    const std::vector<VertexBufferElement>& GetElements() const {
        return elements;
    }
    unsigned GetStride() const {
        return stride;
    }

private:
    std::vector<VertexBufferElement> elements;
    unsigned stride = 0;
};
