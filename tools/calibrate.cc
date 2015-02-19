
#include <pr655.h>

#include <glue/shader.h>
#include <glue/buffer.h>
#include <glue/arrays.h>

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

#include <boost/program_options.hpp>

static const char vs_simple[] = R"SHDR(
#version 330

layout(location = 0) in vec2 pos;

uniform mat4 viewport;

void main()
{
    gl_Position = viewport * vec4(pos, 0.0, 1.0);
}
)SHDR";

static const char fs_simple[] = R"SHDR(
#version 330

out vec4 finalColor;
uniform vec4 plot_color;

void main() {
    finalColor = plot_color;
}
)SHDR";


class looper {
public:

    enum class state : int {
        Waiting = 0,
        NextDisplay = 1,
        NextMeasure = 2,
        Stop = 3
    };

protected:
    virtual bool display() = 0;
    virtual void refresh() = 0;
    virtual void measure() = 0;

public:
    looper() : news(state::Waiting) {}

    bool render() {

        state cur = news;
        if (cur == state::Stop) {
            return false;
        }

        bool do_run = true;
        if (cur == state::NextDisplay) {
            do_run = display();
            state next = do_run ? state::NextMeasure : state::Stop;
            news.compare_exchange_strong(cur, next);
            cur = news;
        }

        if (do_run) {
            refresh();
        }

        return do_run;
    }

    void loop() {
        news = state::NextDisplay;

        state cur_state;
        while ((cur_state = news) != state::Stop) {

            if (cur_state != state::NextMeasure) {
                std::chrono::milliseconds tsleep(100);
                std::this_thread::sleep_for(tsleep);
                continue;
            }

            measure();
            news.compare_exchange_strong(cur_state, state::NextDisplay);
        }
    }

    void start() {

        if (news != state::Waiting) {
            throw std::invalid_argument("wrong state");
        }

        thd = std::thread(&looper::loop, this);

        while(news != state::NextDisplay) {
            std::chrono::milliseconds tsleep(50);
            std::this_thread::sleep_for(tsleep);
        }
    }

    virtual ~looper() {
        if (thd.joinable()) {
            thd.join();
        }
    }

private:
    std::thread        thd;
    std::atomic<state> news;
};

static void error_callback(int error, const char* description)
{
    std::cerr << description << std::endl;
}

static void check_gl_error(const std::string &prefix = "") {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << prefix << "OpenGL error: " << err << std::endl;
    }
}

void wnd_key_cb(GLFWwindow *wnd, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(wnd, GL_TRUE);
    }
}

namespace gl = glue;

struct rgb {

    rgb(float r, float g, float b) : r(r), g(g), b(b), a(1.0f) {}

    union {
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        struct {
            float data[4];
        };
    };
};

class robot : public looper {
public:

    robot(GLFWwindow *wnd, device::pr655 &meter) : window(wnd), meter(meter) {
        init();
    }

    bool display() override {

        if (pos == stim.size()) {
            return false;
        }

        rgb cur = stim[pos++];
        memcpy(color, cur.data, sizeof(cur));
        return true;
    }

    void refresh() override {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        glm::mat4 vp;
        if (height > width) {
            float scale = width / static_cast<float>(height);
            vp = glm::scale(glm::mat4(1), glm::vec3(1.0f, scale, 1.0f));
        } else {
            float scale = height / static_cast<float>(width);
            vp = glm::scale(glm::mat4(1), glm::vec3(scale, 1.0f, 1.0f));
        }

        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        prg.use();
        prg.uniform("plot_color", color);
        prg.uniform("viewport", vp);

        va.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);

        va.unbind();
        prg.unuse();
    }

    void measure() override {
        std::cerr << " Measuring ..." << std::endl;

        meter.measure();
        spectral_data data = meter.spectral();

        resp.push_back(data);
        std::cerr << " done" << std::endl;
    }

    void init() {
        vs = gl::shader::make(vs_simple, GL_VERTEX_SHADER);
        fs = gl::shader::make(fs_simple, GL_FRAGMENT_SHADER);

        vs.compile();
        fs.compile();

        prg = gl::program::make();
        prg.attach({vs, fs});
        prg.link();

        std::vector<float> box = { -0.5f,  0.5f,
                                   -0.5f, -0.5f,
                                   0.5f, -0.5f,

                                   0.5f,  0.5f,
                                   -0.5f,  0.5f,
                                   0.5f, -0.5f};

        bb = gl::buffer::make();
        va = gl::vertex_array::make();

        bb.bind();
        va.bind();

        bb.data(box);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        bb.unbind();
        va.unbind();

        stim = {{1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f, 1.0f}};
        pos = 0;

        stim.reserve(stim.size());
    }

    const std::vector<rgb> & stimulation() const {
        return stim;
    }

    const std::vector<spectral_data> & response() const {
        return resp;
    }

private:
    gl::shader vs;
    gl::shader fs;

    gl::program prg;

    gl::buffer bb;
    gl::vertex_array va;

    GLFWwindow *window;
    device::pr655 &meter;

    // state
    GLfloat color[4];
    size_t pos;

    // data
    std::vector<rgb> stim;
    std::vector<spectral_data> resp;
};

void dump_stdout(const robot &r) {

    const std::vector<rgb> &stim = r.stimulation();
    const std::vector<spectral_data> &resp = r.response();

    if (resp.empty()) {
        std::cout << "No data!" << std::endl;
    }

    std::vector<rgb> RGB = {{1.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f}};

    uint16_t wave = resp[0].wl_start;
    uint16_t step = resp[0].wl_step;
    size_t nwaves = resp[0].data.size();

    const std::string prefix = "# ";

    std::cout << prefix << "spectral data" << std::endl;
    std::cout << prefix << "λ \t   red   \t   green   \t   blue  " << std::endl;

    for (size_t i = 0; i < nwaves; i++) {
        std::cout << wave + i * step << " \t ";

        for (size_t k = 0; k < resp.size(); k++) {
            std::cout << "  " << resp[k].data[i] << "\t";
        }

        std::cout  << std::endl;
    }

}

int main(int argc, char **argv)
{
    namespace po = boost::program_options;

    std::string device;
    std::string mdev;

    po::options_description opts("calibration tool");
    opts.add_options()
            ("help", "produce help message")
            ("device", po::value<std::string>(&device))
            ("monitor", po::value<std::string>(&mdev));

    po::positional_options_description pos;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(opts).positional(pos).run(), vm);
    po::notify(vm);

    if (vm.count("help") > 0) {
        std::cout << opts << std::endl;
        return 0;
    }

    // ****

    device::pr655 meter;

    try {
        meter = device::pr655::open(device);

        meter.start();
        std::cerr << meter.model_number();
        std::cerr << " is ready!" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "E: " << e.what() << std::endl;
        meter.stop();
        return -1;
    }

    // ****

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(error_callback);

    int n_monis;
    GLFWmonitor **monitors = glfwGetMonitors(&n_monis);
    GLFWmonitor *primary = glfwGetPrimaryMonitor();
    GLFWmonitor *mtarget = nullptr;

    for (int i = 0; i < n_monis; i++) {
        const std::string name = glfwGetMonitorName(monitors[i]);
        std::cout << "Monitor: " <<  name << std::endl;
        int phy_width, phy_height;
        glfwGetMonitorPhysicalSize(monitors[i], &phy_width, &phy_height);
        std::cout << "\t size: " << phy_width << "×" << phy_height << " [mm]" << std::endl;

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        const double dpi = mode->width / (phy_width / 25.4);
        std::cout << "\t dpi:  " << dpi  << std::endl;

        if (monitors[i] == primary) {
            std::cout << "\t [primary]" << std::endl;
        }

        if (mtarget == nullptr && name == mdev) {
            mtarget = monitors[i];
            std::cout << "\t [selected]" << std::endl;
        }

        std::cout << std::endl;
    }

    if (!mdev.empty() && mtarget == nullptr) {
        const char *cstr = mdev.c_str();
        char *cend;
        unsigned long k = strtoul(cstr, &cend, 10);
        if (cstr != cend) {
            if (k >= n_monis) {
                std::cerr << "monitor index out of range" << std::endl;
                return -1;
            }

            mtarget = monitors[k];
        }
    }

    if (mtarget == nullptr) {
        mtarget = primary;
    }

    std::cout << "Monitor: " << glfwGetMonitorName(mtarget) << std::endl;

    int n_modes;
    const GLFWvidmode* modes = glfwGetVideoModes(mtarget, &n_modes);

    for (int i = 0; i < n_modes; i++) {
        std::cout << i << ": " << modes[i].width << " × " << modes[i].height;
        std::cout << " \t " << modes[i].redBits << " " << modes[i].greenBits << " " << modes[i].blueBits;
        std::cout << " @ " << modes[i].refreshRate << "Hz" << std::endl;
    }

    const GLFWvidmode *mm = modes + (n_modes - 1);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 2);

    GLFWwindow* window = glfwCreateWindow(mm->width, mm->height, "Calibration", mtarget, nullptr);

    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, wnd_key_cb);

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_MULTISAMPLE);

    // *****
    robot bender(window, meter);

    bender.start();

    bool keep_looping = true;
    while (keep_looping && !glfwWindowShouldClose(window)) {

        keep_looping = bender.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    meter.stop();

    dump_stdout(bender);

    std::cerr << "Goodbay. Have a nice day!" << std::endl;

    glfwTerminate();
    return 0;
}