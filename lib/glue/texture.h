
#ifndef IRIS_TEXTURE_H
#define IRIS_TEXTURE_H

#include <glue/named.h>

namespace glue {

template<typename T = void *>
class texture_t : named<T> {
public:
    texture_t() : named<T>(nullptr) { }

    texture_t(GLuint name) : named<T>(name, delete_name) { }

    static texture_t make() {
        texture_t tex = make_name();
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
        glBindTexture(target, named<T>::name());
    }

    static void parameter(GLenum target, GLenum name, GLfloat value) {
        glTexParameterf(target, name, value);
    }

    static void parameter(GLenum target, GLenum name, GLint value) {
        glTexParameteri(target, name, value);
    }

    static void pixel_store(GLenum name, GLfloat value) {
        glPixelStoref(name, value);
    }

    static void pixel_store(GLenum name, GLint value) {
        glPixelStorei(name, value);
    }
};

typedef texture_t<> texture;

} // glue::

#endif //IRIS_TEXTURE_H
