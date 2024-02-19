#include <jni.h>
#include <string>
#include <cstdint>
#include <unistd.h>
#include <android/input.h>
#include "anbox/graphics/emugl/Renderer.h"
#include "anbox/graphics/emugl/RenderApi.h"
#include "anbox/graphics/emugl/RenderControl.h"
#include "anbox/network/published_socket_connector.h"
#include "anbox/qemu/pipe_connection_creator.h"
#include "anbox/runtime.h"
#include "anbox/common/dispatcher.h"
#include "anbox/input/manager.h"
#include "anbox/input/device.h"
#include "anbox/graphics/layer_composer.h"
#include "anbox/graphics/emugl/DisplayManager.h"
#include "external/android-emugl/shared/emugl/common/logging.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#define TAG "libAnbox"

const char *const path = "/data/data/com.github.ananbox/files";

static const int MAX_FINGERS = 10;
static const int MAX_TRACKING_ID = 10;
static int touch_slots[MAX_FINGERS];
static int last_slot = -1;
static std::shared_ptr<anbox::Runtime> rt;
static std::shared_ptr<anbox::graphics::Rect> frame = std::make_shared<anbox::graphics::Rect>();
static std::shared_ptr<::Renderer> renderer_;
static std::shared_ptr<anbox::network::PublishedSocketConnector> qemu_pipe_connector_;
static std::shared_ptr<anbox::input::Device> touch_;
static ANativeWindow* native_window;


void logger_write(const emugl::LogLevel &level, const char *format, ...) {
    (void)level;

    char message[2048];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message) - 1, format, args);
    va_end(args);

    switch (level) {
        case emugl::LogLevel::WARNING:
            __android_log_print(ANDROID_LOG_WARN, TAG, "%s", message);
            break;
        case emugl::LogLevel::ERROR:
            __android_log_print(ANDROID_LOG_ERROR, TAG, "%s", message);
            break;
        case emugl::LogLevel::FATAL:
            __android_log_print(ANDROID_LOG_FATAL, TAG, "%s", message);
            break;
        case emugl::LogLevel::DEBUG:
            __android_log_print(ANDROID_LOG_DEBUG, TAG, "%s", message);
            break;
        case emugl::LogLevel::TRACE:
//            __android_log_print(ANDROID_LOG_VERBOSE, TAG, "%s", message);
            break;
        default:
            break;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_github_ananbox_Anbox_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_startRuntime(
        JNIEnv *env,
        jobject thiz) {
    rt->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_stopRuntime(JNIEnv *env, jobject thiz) {
    if (rt != nullptr) {
        rt->stop();
        rt = nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_ananbox_Anbox_initRuntime(
        JNIEnv* env,
        jobject thiz,
        jint width,
        jint height,
        jint dpi) {
//    auto gl_libs = anbox::graphics::emugl::default_gl_libraries();
//    if (!anbox::graphics::emugl::initialize(gl_libs, nullptr, nullptr)) {
//        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to initialize OpenGL renderer");
//        return false;
//    }
    if (rt != NULL)
        return false;
    set_emugl_logger(logger_write);
    set_emugl_cxt_logger(logger_write);

    std::uint32_t flags = 0;

    rt = anbox::Runtime::create();

    renderer_ = std::make_shared<::Renderer>();
//    native_window = ANativeWindow_fromSurface(env, surface);
//    int32_t width_ = ANativeWindow_getWidth(native_window);
//    int32_t height_ = ANativeWindow_getHeight(native_window);
    frame->resize(width, height);
    auto display_info_ = anbox::graphics::emugl::DisplayInfo::get();
    display_info_->set_resolution(width, height);
    display_info_->set_dpi(dpi);

    renderer_->initialize(EGL_DEFAULT_DISPLAY);
    registerRenderer(renderer_);

    auto sensors_state = std::make_shared<anbox::application::SensorsState>();
    auto gps_info_broker = std::make_shared<anbox::application::GpsInfoBroker>();

    auto input_manager = std::make_shared<anbox::input::Manager>(rt, anbox::utils::string_format("%s/rootfs/dev/input", path));
//    auto pointer_ = input_manager->create_device();
//    pointer_->set_name("anbox-pointer");
//    pointer_->set_driver_version(1);
//    pointer_->set_input_id({BUS_VIRTUAL, 2, 2, 2});
//    pointer_->set_physical_location("none");
//    pointer_->set_key_bit(BTN_MOUSE);
//    // NOTE: We don't use REL_X/REL_Y in reality but have to specify them here
//    // to allow InputFlinger to detect we're a cursor device.
//    pointer_->set_rel_bit(REL_X);
//    pointer_->set_rel_bit(REL_Y);
//    pointer_->set_rel_bit(REL_HWHEEL);
//    pointer_->set_rel_bit(REL_WHEEL);
//    pointer_->set_prop_bit(INPUT_PROP_POINTER);

//    auto keyboard_ = input_manager->create_device();
//    keyboard_->set_name("anbox-keyboard");
//    keyboard_->set_driver_version(1);
//    keyboard_->set_input_id({BUS_VIRTUAL, 3, 3, 3});
//    keyboard_->set_physical_location("none");
//    keyboard_->set_key_bit(BTN_MISC);
//    keyboard_->set_key_bit(KEY_OK);

    touch_ = input_manager->create_device();
    touch_->set_name("anbox-touch");
    touch_->set_driver_version(1);
    touch_->set_input_id({BUS_VIRTUAL, 4, 4, 4});
    touch_->set_physical_location("none");
    touch_->set_abs_bit(ABS_MT_SLOT);
    touch_->set_abs_max(ABS_MT_SLOT, 10);
    touch_->set_abs_bit(ABS_MT_TOUCH_MAJOR);
    touch_->set_abs_max(ABS_MT_TOUCH_MAJOR, 127);
    touch_->set_abs_bit(ABS_MT_TOUCH_MINOR);
    touch_->set_abs_max(ABS_MT_TOUCH_MINOR, 127);
    touch_->set_abs_bit(ABS_MT_POSITION_X);
    touch_->set_abs_max(ABS_MT_POSITION_X, width);
    touch_->set_abs_bit(ABS_MT_POSITION_Y);
    touch_->set_abs_max(ABS_MT_POSITION_Y, height);
    touch_->set_abs_bit(ABS_MT_TRACKING_ID);
    touch_->set_abs_max(ABS_MT_TRACKING_ID, MAX_TRACKING_ID);
    touch_->set_prop_bit(INPUT_PROP_DIRECT);

    // delete qemu_pipe if exists
    std::string socket_file = anbox::utils::string_format("%s/qemu_pipe", path);
    unlink(socket_file.c_str());
    qemu_pipe_connector_ =
            std::make_shared<anbox::network::PublishedSocketConnector>(
                    anbox::utils::string_format("%s/qemu_pipe", path), rt,
                    std::make_shared<anbox::qemu::PipeConnectionCreator>(renderer_, rt, sensors_state, gps_info_broker));

    return true;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_startContainer(JNIEnv *env, jobject thiz, jstring proot_) {
    char cmd[255];
    if (fork() != 0) {
        return;
    }
    sigset_t signals_to_unblock;
    sigfillset(&signals_to_unblock);
    sigprocmask(SIG_UNBLOCK, &signals_to_unblock, 0);
    const char *proot = env->GetStringUTFChars(proot_, 0);
    sprintf(cmd, "sh %s/rootfs/run.sh %s", path, proot);
    env->ReleaseStringUTFChars(proot_, proot);
    execl("/system/bin/sh", "sh", "-c", cmd, 0);
    __android_log_print(ANDROID_LOG_ERROR, TAG, "proot excuted failed: %s", strerror(errno));
 }
extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_resetWindow(JNIEnv *env, jobject thiz, jint height, jint width) {
    // TODO: check why change frame size cause nothing to be displayed
//    frame->resize(width, height);
    anbox::graphics::emugl::DisplayInfo::get()->set_resolution(height, width);
}

int find_touch_slot(int id){
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (touch_slots[i] == id)
            return i;
    }
    return -1;
}

void push_slot(std::vector<anbox::input::Event> &touch_events, int slot){
    if (last_slot != slot) {
        touch_events.push_back({EV_ABS, ABS_MT_SLOT, slot});
        last_slot = slot;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_pushFingerUp(JNIEnv *env, jobject thiz, jint finger_id) {
    std::vector<anbox::input::Event> touch_events;
    int slot = find_touch_slot(finger_id);
    if (slot == -1)
        return;
    push_slot(touch_events, slot);
    touch_events.push_back({EV_ABS, ABS_MT_TRACKING_ID, -1});
    touch_events.push_back({EV_SYN, SYN_REPORT, 0});
    touch_slots[slot] = -1;
    touch_->send_events(touch_events);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_pushFingerDown(JNIEnv *env, jobject thiz, jint x, jint y, jint finger_id) {
    std::vector<anbox::input::Event> touch_events;
    int slot = find_touch_slot(-1);
    if (slot == -1) {
        DEBUG("no free slot!");
        return;
    }
    touch_slots[slot] = finger_id;
    push_slot(touch_events, slot);
    touch_events.push_back({EV_ABS, ABS_MT_TRACKING_ID, static_cast<std::int32_t>(finger_id % MAX_TRACKING_ID + 1)});
    touch_events.push_back({EV_ABS, ABS_MT_POSITION_X, x});
    touch_events.push_back({EV_ABS, ABS_MT_POSITION_Y, y});
    touch_events.push_back({EV_SYN, SYN_REPORT, 0});
    touch_->send_events(touch_events);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_pushFingerMotion(JNIEnv *env, jobject thiz, jint x, jint y,
                                               jint finger_id) {
    std::vector<anbox::input::Event> touch_events;
    int slot = find_touch_slot(finger_id);
    if (slot == -1)
        return;
    push_slot(touch_events, slot);
    touch_events.push_back({EV_ABS, ABS_MT_POSITION_X, x});
    touch_events.push_back({EV_ABS, ABS_MT_POSITION_Y, y});
    touch_events.push_back({EV_SYN, SYN_REPORT, 0});
    touch_->send_events(touch_events);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_destroyWindow(JNIEnv *env, jobject thiz) {
//    getRenderer()->destroyAllNativeWindow();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_createSurface(JNIEnv *env, jobject thiz, jobject surface) {
    native_window = ANativeWindow_fromSurface(env, surface);
    renderer_->createNativeWindow(native_window);
    auto composer_ = std::make_shared<anbox::graphics::LayerComposer>(renderer_, frame, native_window);
    registerLayerComposer(composer_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_destroySurface(JNIEnv *env, jobject thiz) {
    unRegisterLayerComposer();
    renderer_->destroyNativeWindow(native_window);
    ANativeWindow_release(native_window);
    native_window = NULL;
}