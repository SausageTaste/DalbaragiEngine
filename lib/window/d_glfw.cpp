#include "d_glfw.h"

#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <fmt/format.h>

#include "d_timer.h"
#include "d_logger.h"


namespace {

    // The result might be nullptr
    GLFWwindow* create_glfw_window(const unsigned width, const unsigned height, const char* const title) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        return glfwCreateWindow(width, height, title, nullptr, nullptr);
    }

    VkSurfaceKHR create_vk_surface(const VkInstance instance, GLFWwindow* const window) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        const auto create_result = glfwCreateWindowSurface(
            instance,
            window,
            nullptr,
            &surface
        );

        if (VK_SUCCESS != create_result)
            return nullptr;

        if (VK_NULL_HANDLE == surface)
            return nullptr;

        return surface;
    }

}


namespace {

    std::function<void(int, int)> g_callback_func_buf_resize;

    static void callback_fbuf_resize(GLFWwindow* window, int width, int height) {
        if (g_callback_func_buf_resize)
            g_callback_func_buf_resize(width, height);
    }


    std::function<void(const dal::MouseEvent&)> g_callback_func_mouse_event;

    void callback_cursor_pos(GLFWwindow* window, double xpos, double ypos) {
        if (!g_callback_func_mouse_event)
            return;

        dal::MouseEvent e;
        e.m_action_type = dal::MouseActionType::move;
        e.m_time_sec = dal::get_cur_sec();
        e.m_pos = glm::vec2{ xpos, ypos };

        g_callback_func_mouse_event(e);
    }

    void callback_mouse_button(GLFWwindow* window, int button, int action, int mods) {
        if (!g_callback_func_mouse_event)
            return;

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        dal::MouseEvent e;
        e.m_time_sec = dal::get_cur_sec();
        e.m_pos = glm::vec2{ xpos, ypos };

        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                e.m_button = dal::MouseButton::left;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                e.m_button = dal::MouseButton::right;
                break;
        }

        switch (action) {
            case GLFW_PRESS:
                e.m_action_type = dal::MouseActionType::down;
                break;
            case GLFW_RELEASE:
                e.m_action_type = dal::MouseActionType::up;
                break;
        }

        g_callback_func_mouse_event(e);
    }


    std::function<void(const dal::KeyEvent&)> g_callback_func_key_event;

    dal::KeyCode map_glfw_key_id(const int glfw_key) {
        if (GLFW_KEY_A <= glfw_key && glfw_key <= GLFW_KEY_Z) {
            auto index = glfw_key - GLFW_KEY_A + static_cast<int>(dal::KeyCode::a);
            return static_cast<dal::KeyCode>(index);
        }
        else if (GLFW_KEY_0 <= glfw_key && glfw_key <= GLFW_KEY_9) {
            auto index = glfw_key - GLFW_KEY_0 + static_cast<int>(dal::KeyCode::n0);
            return static_cast<dal::KeyCode>(index);
        }
        else {
            static const std::unordered_map<int, dal::KeyCode> map{
                {GLFW_KEY_GRAVE_ACCENT,  dal::KeyCode::backquote},
                {GLFW_KEY_MINUS,         dal::KeyCode::minus},
                {GLFW_KEY_EQUAL,         dal::KeyCode::equal},
                {GLFW_KEY_LEFT_BRACKET,  dal::KeyCode::lbracket},
                {GLFW_KEY_RIGHT_BRACKET, dal::KeyCode::rbracket},
                {GLFW_KEY_BACKSLASH,     dal::KeyCode::backslash},
                {GLFW_KEY_SEMICOLON,     dal::KeyCode::semicolon},
                {GLFW_KEY_APOSTROPHE,    dal::KeyCode::quote},
                {GLFW_KEY_COMMA,         dal::KeyCode::comma},
                {GLFW_KEY_PERIOD,        dal::KeyCode::period},
                {GLFW_KEY_SLASH,         dal::KeyCode::slash},

                {GLFW_KEY_SPACE,         dal::KeyCode::space},
                {GLFW_KEY_ENTER,         dal::KeyCode::enter},
                {GLFW_KEY_BACKSPACE,     dal::KeyCode::backspace},
                {GLFW_KEY_TAB,           dal::KeyCode::tab},

                {GLFW_KEY_ESCAPE,        dal::KeyCode::escape},
                {GLFW_KEY_LEFT_SHIFT,    dal::KeyCode::lshfit},
                {GLFW_KEY_RIGHT_SHIFT,   dal::KeyCode::rshfit},
                {GLFW_KEY_LEFT_CONTROL,  dal::KeyCode::lctrl},
                {GLFW_KEY_RIGHT_CONTROL, dal::KeyCode::rctrl},
                {GLFW_KEY_LEFT_ALT,      dal::KeyCode::lalt},
                {GLFW_KEY_RIGHT_ALT,     dal::KeyCode::ralt},
                {GLFW_KEY_UP,            dal::KeyCode::up},
                {GLFW_KEY_DOWN,          dal::KeyCode::down},
                {GLFW_KEY_LEFT,          dal::KeyCode::left},
                {GLFW_KEY_RIGHT,         dal::KeyCode::right},
            };

            const auto res = map.find(glfw_key);
            if (res == map.end()) {
                return dal::KeyCode::unknown;
            }
            else {
                return res->second;
            }
        }
    }

    void callback_key_event(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (!g_callback_func_key_event)
            return;

        dal::KeyEvent e;

        e.m_time_sec = dal::get_cur_sec();
        e.m_key = ::map_glfw_key_id(key);

        e.reset_modifier_states();
        if (GLFW_MOD_SHIFT & mods)
            e.set_modifier_state(dal::KeyModifier::shift, true);
        if (GLFW_MOD_CONTROL & mods)
            e.set_modifier_state(dal::KeyModifier::ctrl, true);
        if (GLFW_MOD_ALT & mods)
            e.set_modifier_state(dal::KeyModifier::alt, true);
        if (GLFW_MOD_CAPS_LOCK & mods)
            e.set_modifier_state(dal::KeyModifier::caps_lock, true);
        if (GLFW_MOD_NUM_LOCK & mods)
            e.set_modifier_state(dal::KeyModifier::num_lock, true);

        switch (action) {
            case GLFW_PRESS:
                e.m_action_type = dal::KeyActionType::down;
                break;
            case GLFW_REPEAT:
                e.m_action_type = dal::KeyActionType::down;
                break;
            case GLFW_RELEASE:
                e.m_action_type = dal::KeyActionType::up;
                break;
            default:
                dalWarn(fmt::format("Unknown key action type: {}", action).c_str());
        }

        g_callback_func_key_event(e);
    }


    std::function<void(const dal::GamepadConnectionEvent&)> g_callback_func_gamepad_connection;

    dal::GamepadConnectionEvent make_gamepad_connection_event(const int jid) {
        dal::GamepadConnectionEvent e;
        e.m_id = jid;

        if (GLFW_TRUE == glfwJoystickPresent(jid)) {
            if (GLFW_TRUE != glfwJoystickIsGamepad(jid)) {
                e.m_connected = false;
                return e;
            }

            e.m_connected = true;
            e.m_name = glfwGetJoystickName(jid);
        }

        return e;
    }

    void joystick_callback(const int jid, const int event) {
        if (!g_callback_func_gamepad_connection)
            return;

        const auto e = ::make_gamepad_connection_event(jid);
        g_callback_func_gamepad_connection(e);
    }

}


namespace dal {

    WindowGLFW::WindowGLFW(const char* const title) {
        this->m_title = title;

        const auto window = ::create_glfw_window(800, 450, title);
        if (nullptr == window) {
            throw std::runtime_error{ "Failed to create glfw window" };
        }
        this->m_window = window;

        glfwSetFramebufferSizeCallback(window, ::callback_fbuf_resize);
        glfwSetCursorPosCallback(window, ::callback_cursor_pos);
        glfwSetMouseButtonCallback(window, ::callback_mouse_button);
        glfwSetKeyCallback(window, ::callback_key_event);
        glfwSetJoystickCallback(::joystick_callback);
    }

    WindowGLFW::~WindowGLFW() {
        glfwDestroyWindow(reinterpret_cast<GLFWwindow*>(this->m_window));
        this->m_window = nullptr;
        glfwTerminate();
    }

    void WindowGLFW::do_frame() {
        glfwPollEvents();
    }

    bool WindowGLFW::should_close() const {
        return glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(this->m_window));
    }

    std::vector<const char*> WindowGLFW::get_vulkan_extensions() const {
        uint32_t ext_count = 0;
        const char** glfw_extensions = nullptr;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&ext_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + ext_count);

        return extensions;
    }

    std::function<void*(void*)> WindowGLFW::get_vk_surface_creator() const {
        return [this](void* vk_instance) -> void* {
            return ::create_vk_surface(
                reinterpret_cast<VkInstance>(vk_instance),
                reinterpret_cast<GLFWwindow*>(this->m_window)
            );
        };
    }

    void WindowGLFW::set_callback_fbuf_resize(std::function<void(int, int)> func) {
        g_callback_func_buf_resize = func;
    }

    void WindowGLFW::set_callback_mouse_event(std::function<void(const dal::MouseEvent&)> func) {
        g_callback_func_mouse_event = func;
    }

    void WindowGLFW::set_callback_key_event(std::function<void(const dal::KeyEvent&)> func) {
        g_callback_func_key_event = func;
    }

    void WindowGLFW::set_callback_gamepad_connection(std::function<void(const dal::GamepadConnectionEvent&)> func) {
        g_callback_func_gamepad_connection = func;

        for (auto i = GLFW_JOYSTICK_1; i < GLFW_JOYSTICK_LAST; ++i) {
            const bool present = 0 != glfwJoystickPresent(i);
            if (present) {
                const auto e = ::make_gamepad_connection_event(i);
                if (e.m_connected)
                    func(e);
            }
        }
    }

    void WindowGLFW::update_input_gamepad(GamepadInputManager& gamepad_manager) const {
        for (auto& [id, state] : gamepad_manager) {
            const bool is_present = GLFW_TRUE == glfwJoystickPresent(id);
            if (!is_present) {
                gamepad_manager.remove_gamepad(id);
                continue;
            }

            GLFWgamepadstate state;
            if (GLFW_TRUE == glfwGetGamepadState(id, &state)) {
                dalInfo(fmt::format("{} x {}", state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]).c_str());
            }
        }
    }

    uint32_t WindowGLFW::width() const {
        int width = 0;
        glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(this->m_window), &width, nullptr);
        return width;
    }

    uint32_t WindowGLFW::height() const {
        int height = 0;
        glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(this->m_window), nullptr, &height);
        return height;
    }

}
