
#ifndef IRIS_TEXTURE_H
#define IRIS_TEXTURE_H

#include <glue/named.h>

namespace glue {

class texture : named<texture> {
public:
    texture() : named(nullptr) { }

    texture(GLuint name) : named(name) { }

    static texture make() {
        texture tex = make_name();
        return tex;
    }

    static GLuint make_name() {
        GLuint name = 0;
        glGenTextures(1, &name);
        return name;
    }

    static void delete_name(GLuint name) {
        glDeleteTextures(1, &name);
    }

    void bind(GLenum target) {
        glBindTexture(target, name());
    }

};

} // glue::

#endif //IRIS_TEXTURE_H