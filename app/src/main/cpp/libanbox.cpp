#include <jni.h>
#include <string>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <android/input.h>
#include "anbox/graphics/emugl/Renderer.h"
#include "anbox/graphics/emugl/RenderApi.h"
#include "anbox/graphics/emugl/RenderControl.h"
#include "anbox/network/published_socket_connector.h"
#include "anbox/qemu/pipe_connection_creator.h"
#include "anbox/runtime.h"
#include "anbox/utils.h"
#include "anbox/common/dispatcher.h"
#include "anbox/input/manager.h"
#include "anbox/input/device.h"
#include "anbox/graphics/layer_composer.h"
#include "anbox/graphics/emugl/DisplayManager.h"
#include "external/android-emugl/shared/emugl/common/logging.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include "Parcel.h"
#include "AnanboxFile.h"
#define TAG "libAnbox"

#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define __android_second(dummy, second, ...)     second
#define __android_rest(first, ...)               , ## __VA_ARGS__
#define android_printAssert(cond, tag, fmt...) \
    __android_log_assert(cond, tag, \
        __android_second(0, ## fmt, NULL) __android_rest(fmt))

#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))
#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)android_printAssert(#cond, TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

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
static char path[255];

// Handle to the process stopContainer() has to tear down. Only set by
// startContainer() and consumed by stopContainer(), both on the UI thread.
static pid_t container_pid = -1;

// proot's event loop ignores every terminating signal (SIGTERM/SIGINT are set
// to SIG_IGN), and its --kill-on-exit cleanup only runs on a normal exit or on
// SIGQUIT/SIGILL/SIGABRT/SIGFPE/SIGSEGV. Anything else leaves the guest's init
// and its children orphaned under /proc. See tracee/event.c in proot.
static const int PROOT_QUIT = SIGQUIT;

// Blocks until the forked shell exits. No polling: the shell is our child, so
// waitpid() parks us until it is reaped. SIGQUIT shuts down the container
// cleanly; SIGKILL is only for a proot wedged past the timeout.
static void wait_for_exit(pid_t pid, int timeout_ms) {
    int status = 0;
    for (int waited = 0; waited < timeout_ms; waited += 100) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) {
            if (WIFEXITED(status))
                ALOGI("container host %d exited with status %d", (int)pid, WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                ALOGI("container host %d killed by signal %d", (int)pid, WTERMSIG(status));
            return;
        }
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            // ECHILD: already reaped. Nothing left to wait for.
            return;
        }
        usleep(100 * 1000);
    }
    ALOGW("container host %d still running, killing it", (int)pid);
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

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
JNIEXPORT jint JNICALL
Java_com_github_ananbox_Anbox_startContainer(JNIEnv *env, jobject thiz, jstring proot_) {
    if (container_pid > 0) {
        ALOGW("container already running as %d", (int)container_pid);
        return (jint)container_pid;
    }

    const char *proot = env->GetStringUTFChars(proot_, 0);
    // Exec the script directly rather than through "sh -c": mksh forks a
    // wrapper shell for the -c form, and that wrapper would be the pid we
    // signal, while only the script below it carries proot and its trap.
    std::string script = anbox::utils::string_format("%s/rootfs/run.sh", path);
    std::string data_dir = path;
    std::string proot_bin = proot;
    env->ReleaseStringUTFChars(proot_, proot);

    // Captured before fork(): in the child this is the parent's pid, which is
    // what PR_SET_PDEATHSIG tracks. Using getpid() in the child would compare
    // the child with itself and always look orphaned.
    pid_t parent = getpid();

    pid_t pid = fork();
    if (pid != 0) {
        if (pid > 0) {
            container_pid = pid;
            ALOGI("container host started as %d", (int)pid);
        } else {
            ALOGE("failed to fork container: %s", strerror(errno));
        }
        return (jint)pid;
    }

    sigset_t signals_to_unblock;
    sigfillset(&signals_to_unblock);
    sigprocmask(SIG_UNBLOCK, &signals_to_unblock, 0);

    // Last-resort cleanup for an app that dies without calling
    // stopContainer() (kmem kill, force-stop, crash): the kernel signals us as
    // soon as the forking thread goes away, so the container cannot outlive
    // it. The setting survives execve, so the shell below inherits it.
    //
    // SIGQUIT, not SIGKILL: the shell forwards it to proot, whose handler runs
    // the --kill-on-exit cleanup (init and everything else go with it). A
    // SIGKILL would only take out the shell and orphan proot, which ignores
    // every other terminating signal. See tracee/event.c in proot.
    if (prctl(PR_SET_PDEATHSIG, PROOT_QUIT) != 0)
        __android_log_print(ANDROID_LOG_ERROR, TAG, "PR_SET_PDEATHSIG failed: %s", strerror(errno));
    // The parent may have died between fork() and the prctl() above, in which
    // case no death signal will ever be delivered; check and bail out.
    if (getppid() != parent) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "parent %d gone before exec, aborting", (int)parent);
        _exit(0);
    }

    execl("/system/bin/sh", "sh", script.c_str(), data_dir.c_str(),
          proot_bin.c_str(), (char *)0);
    __android_log_print(ANDROID_LOG_ERROR, TAG, "proot excuted failed: %s", strerror(errno));
    _exit(127);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_stopContainer(JNIEnv *env, jobject thiz) {
    if (container_pid <= 0) {
        ALOGI("no container to stop");
        return;
    }

    pid_t pid = container_pid;
    container_pid = -1;
    ALOGI("stopping container %d", (int)pid);

    // SIGQUIT is forwarded by run.sh to proot, whose handler kills every
    // tracee (init included) before it exits. SIGTERM would be silently
    // ignored by proot's event loop and leave the guest running.
    if (kill(pid, PROOT_QUIT) != 0 && errno != ESRCH) {
        ALOGE("failed to signal container %d: %s", (int)pid, strerror(errno));
        return;
    }

    // run.sh is waiting on proot and exits right after it, and so does the
    // shell we forked, so this bound is only a safety net.
    wait_for_exit(pid, 10000);
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

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_setPath(JNIEnv *env, jobject thiz, jstring path_) {
    const char *pathStr = env->GetStringUTFChars(path_, 0);
    memcpy(path, pathStr, strlen(pathStr) + 1);
    env->ReleaseStringUTFChars(path_, pathStr);
}

static inline jclass FindClassOrDie(JNIEnv* env, const char* class_name) {
    jclass clazz = env->FindClass(class_name);
    LOG_ALWAYS_FATAL_IF(clazz == NULL, "Unable to find class %s", class_name);
    return clazz;
}


static inline jfieldID GetFieldIDOrDie(JNIEnv* env, jclass clazz, const char* field_name,
                                       const char* field_signature) {
    jfieldID res = env->GetFieldID(clazz, field_name, field_signature);
    LOG_ALWAYS_FATAL_IF(res == NULL,"Unable to find field %s with signature %s", field_name,field_signature);
    return res;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_ananbox_Anbox_dumpParcel(JNIEnv *env, jobject thiz, jobject jparcel, jstring jpath) {
    const char* const kParcelPathName = "android/os/Parcel";
    jclass parcel_clazz = FindClassOrDie(env, kParcelPathName);
    jfieldID  parcel_mNativePtr = GetFieldIDOrDie(env, parcel_clazz, "mNativePtr", "J");

    Parcel* parcel = (Parcel*) env->GetLongField(jparcel, parcel_mNativePtr);
    if (parcel == nullptr) {
        ALOGI("error,  Parcel/mNativePtr is null");
        return;
    }

    ALOGI("mdata: %p", parcel->mData);
    ALOGI("mdataPos: %zu", parcel->mDataPos);
    ALOGI("mdataSize: %zu", parcel->mDataSize);
    ALOGI("mdataCapacity: %zu", parcel->mDataCapacity);
    ALOGI("mObject: %llu", (unsigned long long) *parcel->mObjects);
    ALOGI("mObjectSize: %zu", parcel->mObjectsSize);

    const char *path = env->GetStringUTFChars(jpath, 0);
    int fd = open(path, O_CREAT | O_WRONLY, 0700);
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "failed to open file, err :%d", errno);
    }
    else {
        write(fd, &parcel->mDataSize, sizeof(int64_t));
        write(fd, &parcel->mObjectsSize, sizeof(int64_t));
        write(fd, parcel->mData, parcel->mDataSize);
        write(fd, parcel->mObjects, parcel->mObjectsSize * sizeof(int64_t));
        close(fd);
    }
    env->ReleaseStringUTFChars(jpath, path);
}

static Parcel* getNativeParcelOrDie(JNIEnv *env, jobject jparcel) {
    if (jparcel == nullptr)
        return nullptr;
    jclass parcel_clazz = FindClassOrDie(env, "android/os/Parcel");
    jfieldID  parcel_mNativePtr = GetFieldIDOrDie(env, parcel_clazz, "mNativePtr", "J");
    return (Parcel*) env->GetLongField(jparcel, parcel_mNativePtr);
}

// Number of flat_binder_object entries currently recorded in the parcel.
// Used by the Kotlin side to learn the index of the binder it just wrote, so
// the guest never has to scan the parcel looking for it.
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_ananbox_Anbox_getParcelObjectsCount(JNIEnv *env, jobject thiz, jobject jparcel) {
    Parcel* parcel = getNativeParcelOrDie(env, jparcel);
    if (parcel == nullptr)
        return 0;
    return (jint) parcel->mObjectsSize;
}

static bool writeAll(int fd, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*) buf;
    while (len > 0) {
        ssize_t written = write(fd, p, len);
        if (written <= 0) {
            if (written < 0 && errno == EINTR)
                continue;
            return false;
        }
        p += written;
        len -= (size_t) written;
    }
    return true;
}

// Dump the raw transact parcel in the format described by AnanboxFile.h.
//
// binderObjectIndex is the index (into the Parcel object offset table) of the
// flat_binder_object that the guest must rebind to its LocalBinder. A negative
// value is recorded as ANANBOX_NO_BINDER_INDEX.
// transactCode is the transaction code the guest should call; <= 0 means the
// guest should refuse to use the file.
//
// The file is written to a temporary name and renamed into place, so a guest
// reading concurrently never sees a partial file (the old code used O_CREAT
// without O_TRUNC and could leave stale trailing bytes behind).
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_ananbox_Anbox_dumpRawParcel(JNIEnv *env, jobject thiz, jobject jparcel,
                                            jstring jpath, jint binderObjectIndex,
                                            jint transactCode) {
    Parcel* parcel = getNativeParcelOrDie(env, jparcel);
    if (parcel == nullptr || parcel->mData == nullptr) {
        ALOGI("dumpRawParcel: parcel or data is null");
        return JNI_FALSE;
    }
    if (binderObjectIndex >= 0 && (size_t) binderObjectIndex >= parcel->mObjectsSize) {
        ALOGI("dumpRawParcel: binder_object_index %d out of range (objects=%zu)",
              binderObjectIndex, parcel->mObjectsSize);
        return JNI_FALSE;
    }
    if (parcel->mObjectsSize > 0 && parcel->mObjects == nullptr) {
        ALOGI("dumpRawParcel: objects table is null but objects_size is %zu",
              parcel->mObjectsSize);
        return JNI_FALSE;
    }

    ananbox_parcel_header header;
    memset(&header, 0, sizeof(header));
    header.magic = ANANBOX_PARCEL_MAGIC;
    header.version = ANANBOX_PARCEL_VERSION;
    header.transact_code = transactCode;
    header.binder_object_index = binderObjectIndex < 0
            ? ANANBOX_NO_BINDER_INDEX : (uint32_t) binderObjectIndex;
    header.data_size = (uint32_t) parcel->mDataSize;
    header.objects_count = (uint32_t) parcel->mObjectsSize;
    header.objects_elem_size = (uint32_t) sizeof(binder_size_t);

    const char *path = env->GetStringUTFChars(jpath, 0);
    std::string tmp_path = std::string(path) + ".tmp";
    jboolean ok = JNI_FALSE;

    int fd = open(tmp_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) {
        ALOGI("dumpRawParcel: open %s failed, errno=%d", tmp_path.c_str(), errno);
    } else {
        bool written = writeAll(fd, &header, sizeof(header))
                && writeAll(fd, parcel->mData, parcel->mDataSize)
                && writeAll(fd, parcel->mObjects,
                            parcel->mObjectsSize * sizeof(binder_size_t));
        if (written) {
            fsync(fd);
            close(fd);
            if (rename(tmp_path.c_str(), path) == 0) {
                ok = JNI_TRUE;
            } else {
                ALOGI("dumpRawParcel: rename to %s failed, errno=%d", path, errno);
                unlink(tmp_path.c_str());
            }
        } else {
            ALOGI("dumpRawParcel: write failed, errno=%d", errno);
            close(fd);
            unlink(tmp_path.c_str());
        }
    }

    env->ReleaseStringUTFChars(jpath, path);
    return ok;
}