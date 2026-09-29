#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <optional>
#include <pwd.h>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

#include "core/log.h"

#include "service/polkit_service.h"

namespace astralia {

namespace {

constexpr const char *agent_object_path = "/org/astralia_shell/PolkitAuthenticationAgent";

template <typename F>
void guard_callback(const char *name, F &&body) noexcept {
    try {
        std::forward<F>(body)();
    } catch (const std::exception &e) {
        log::error("polkit: exception in callback {} ({})", name, e.what());
    } catch (...) {
        log::error("polkit: unknown exception in callback {}", name);
    }
}

std::string identity_name(PolkitIdentity *identity) {
    if (POLKIT_IS_UNIX_USER(identity)) {
        auto uid = static_cast<uid_t>(polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity)));
        passwd pwd{};
        passwd *result = nullptr;
        std::array<char, 4096> buffer{};
        if (getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &result) == 0 &&
            result != nullptr && result->pw_name != nullptr && result->pw_name[0] != '\0') {
            return result->pw_name;
        }
        return std::to_string(uid);
    }
    if (POLKIT_IS_UNIX_GROUP(identity)) {
        return "group " + std::to_string(polkit_unix_group_get_gid(POLKIT_UNIX_GROUP(identity)));
    }
    return "unknown";
}

class IdentityRef {
  public:
    explicit IdentityRef(PolkitIdentity *identity = nullptr) : identity_(identity) {
        if (identity_ != nullptr) {
            g_object_ref(identity_);
        }
    }

    ~IdentityRef() {
        if (identity_ != nullptr) {
            g_object_unref(identity_);
        }
    }

    IdentityRef(const IdentityRef &) = delete;
    IdentityRef &operator=(const IdentityRef &) = delete;

    IdentityRef(IdentityRef &&other) noexcept
        : identity_(std::exchange(other.identity_, nullptr)) {}

    IdentityRef &operator=(IdentityRef &&other) noexcept {
        if (this != &other) {
            if (identity_ != nullptr) {
                g_object_unref(identity_);
            }
            identity_ = std::exchange(other.identity_, nullptr);
        }
        return *this;
    }

    PolkitIdentity *get() const { return identity_; }

  private:
    PolkitIdentity *identity_ = nullptr;
};

struct AuthRequest {
    std::string action_id;
    std::string message;
    std::string cookie;
    std::vector<IdentityRef> identities;
    GTask *task = nullptr;
    GCancellable *cancellable = nullptr;
    gulong cancel_handler = 0;
    bool finished = false;

    ~AuthRequest() {
        if (cancellable != nullptr && cancel_handler != 0) {
            g_cancellable_disconnect(cancellable, cancel_handler);
        }
        if (cancellable != nullptr) {
            g_object_unref(cancellable);
        }
        if (!finished && task != nullptr) {
            g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "%s",
                                    "Authentication request was destroyed");
        }
        if (task != nullptr) {
            g_object_unref(task);
        }
    }

    void complete() {
        if (finished || task == nullptr) {
            return;
        }
        finished = true;
        g_task_return_boolean(task, TRUE);
    }

    void cancel(const char *reason) {
        if (finished || task == nullptr) {
            return;
        }
        finished = true;
        g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "%s", reason);
    }
};

using InitiateBridge = void (*)(void *, std::unique_ptr<AuthRequest>);
using CancelBridge = void (*)(void *, AuthRequest *);

struct AstraliaPolkitListener {
    PolkitAgentListener parent_instance;
    void *owner;
    InitiateBridge initiate;
    CancelBridge cancel;
    gpointer registration;
};

struct AstraliaPolkitListenerClass {
    PolkitAgentListenerClass parent_class;
};

G_DEFINE_TYPE(AstraliaPolkitListener, astralia_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

void request_cancelled(GCancellable *, gpointer user_data) noexcept {
    guard_callback("request_cancelled", [&] {
        auto *request = static_cast<AuthRequest *>(user_data);
        request->cancel_handler = 0;
        gpointer source = G_IS_TASK(request->task) ? g_task_get_source_object(request->task) : nullptr;
        auto *listener = static_cast<AstraliaPolkitListener *>(source);
        if (listener != nullptr && listener->cancel != nullptr && listener->owner != nullptr) {
            listener->cancel(listener->owner, request);
        }
    });
}

void initiate_authentication(PolkitAgentListener *listener, const gchar *action_id,
                             const gchar *message, const gchar *, PolkitDetails *,
                             const gchar *cookie, GList *identities, GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data) noexcept {
    guard_callback("initiate_authentication", [&] {
        auto *self = reinterpret_cast<AstraliaPolkitListener *>(listener);
        auto request = std::make_unique<AuthRequest>();
        request->action_id = action_id != nullptr ? action_id : "";
        request->message = message != nullptr ? message : "";
        request->cookie = cookie != nullptr ? cookie : "";
        request->task = g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
        request->cancellable =
            cancellable != nullptr ? static_cast<GCancellable *>(g_object_ref(cancellable)) : nullptr;
        for (GList *item = g_list_first(identities); item != nullptr; item = g_list_next(item)) {
            auto *identity = static_cast<PolkitIdentity *>(item->data);
            if (identity == nullptr) {
                continue;
            }
            bool duplicate = std::ranges::any_of(request->identities, [identity](const IdentityRef &existing) {
                return polkit_identity_equal(existing.get(), identity);
            });
            if (!duplicate) {
                request->identities.emplace_back(identity);
            }
        }
        if (cancellable != nullptr) {
            request->cancel_handler = g_cancellable_connect(
                cancellable, G_CALLBACK(request_cancelled), request.get(), nullptr);
        }
        if (self->initiate == nullptr || self->owner == nullptr) {
            request->cancel("Polkit listener is not attached");
            return;
        }
        self->initiate(self->owner, std::move(request));
    });
}

gboolean initiate_authentication_finish(PolkitAgentListener *, GAsyncResult *result,
                                        GError **error) {
    return g_task_propagate_boolean(G_TASK(result), error);
}

void astralia_polkit_listener_init(AstraliaPolkitListener *self) {
    self->owner = nullptr;
    self->initiate = nullptr;
    self->cancel = nullptr;
    self->registration = nullptr;
}

void astralia_polkit_listener_class_init(AstraliaPolkitListenerClass *klass) {
    auto *listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
    listener_class->initiate_authentication = initiate_authentication;
    listener_class->initiate_authentication_finish = initiate_authentication_finish;
}

} // namespace

struct PolkitService::Impl {
    std::function<void()> on_change;
    AstraliaPolkitListener *listener = nullptr;
    PolkitAgentSession *session = nullptr;
    GMainContext *context = g_main_context_default();
    GCancellable *register_cancellable = nullptr;
    std::unique_ptr<AuthRequest> pending;
    bool cancelling = false;
    bool response_required = false;
    std::string info;
    bool info_error = false;
    std::vector<GPollFD> poll_fds;
    gint max_priority = G_PRIORITY_DEFAULT;

    explicit Impl(std::function<void()> callback) : on_change(std::move(callback)) {
        listener = static_cast<AstraliaPolkitListener *>(
            g_object_new(astralia_polkit_listener_get_type(), nullptr));
        listener->owner = this;
        listener->initiate = &Impl::initiate_bridge;
        listener->cancel = &Impl::cancel_bridge;
    }

    ~Impl() {
        clear_pending("Authentication agent is shutting down", true);
        if (register_cancellable != nullptr) {
            g_cancellable_cancel(register_cancellable);
            g_object_unref(register_cancellable);
        }
        listener->owner = nullptr;
        listener->initiate = nullptr;
        listener->cancel = nullptr;
        if (listener->registration != nullptr) {
            polkit_agent_listener_unregister(listener->registration);
        }
        g_object_unref(listener);
    }

    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    void start() {
        const char *session_id = std::getenv("XDG_SESSION_ID");
        if (session_id != nullptr && session_id[0] != '\0') {
            register_subject(polkit_unix_session_new(session_id), nullptr);
            return;
        }
        register_cancellable = g_cancellable_new();
        polkit_unix_session_new_for_process(getpid(), register_cancellable, &Impl::session_ready, this);
    }

    static void session_ready(GObject *, GAsyncResult *result, gpointer user_data) noexcept {
        guard_callback("session_ready", [&] {
            static_cast<Impl *>(user_data)->on_session_ready(result);
        });
    }

    void on_session_ready(GAsyncResult *result) {
        GError *error = nullptr;
        PolkitSubject *subject = polkit_unix_session_new_for_process_finish(result, &error);
        if (error != nullptr && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            if (subject != nullptr) {
                g_object_unref(subject);
            }
            return;
        }
        if (error != nullptr && std::string_view(error->message).contains("No session for pid")) {
            g_clear_error(&error);
            if (subject != nullptr) {
                g_object_unref(subject);
            }
            log::info("polkit: no logind session for pid, registering for the unix user");
            subject = POLKIT_SUBJECT(polkit_unix_user_new(static_cast<gint>(getuid())));
        }
        register_subject(subject, error);
    }

    void register_subject(PolkitSubject *subject, GError *error) {
        if (register_cancellable != nullptr) {
            g_object_unref(register_cancellable);
            register_cancellable = nullptr;
        }
        if (subject == nullptr || error != nullptr) {
            log::error("polkit: {}", error != nullptr ? error->message : "cannot create the session subject");
            g_clear_error(&error);
            if (subject != nullptr) {
                g_object_unref(subject);
            }
            return;
        }
        GError *register_error = nullptr;
        gpointer handle = polkit_agent_listener_register(
            POLKIT_AGENT_LISTENER(listener), POLKIT_AGENT_REGISTER_FLAGS_NONE, subject,
            agent_object_path, nullptr, &register_error);
        g_object_unref(subject);
        if (register_error != nullptr || handle == nullptr) {
            log::error("polkit: cannot register the agent: {}",
                       register_error != nullptr ? register_error->message : "no handle");
            g_clear_error(&register_error);
            if (handle != nullptr) {
                polkit_agent_listener_unregister(handle);
            }
            return;
        }
        listener->registration = handle;
        log::info("polkit: registered authentication agent at {}", agent_object_path);
    }

    static void initiate_bridge(void *owner, std::unique_ptr<AuthRequest> request) {
        static_cast<Impl *>(owner)->begin(std::move(request));
    }

    static void cancel_bridge(void *owner, AuthRequest *request) {
        static_cast<Impl *>(owner)->cancel_from_authority(request);
    }

    static void completed(PolkitAgentSession *, gboolean gained, gpointer user_data) noexcept {
        guard_callback("completed", [&] { static_cast<Impl *>(user_data)->on_completed(gained != FALSE); });
    }

    static void request(PolkitAgentSession *, gchar *, gboolean, gpointer user_data) noexcept {
        guard_callback("request", [&] {
            auto *self = static_cast<Impl *>(user_data);
            self->response_required = true;
            self->changed();
        });
    }

    static void show_error(PolkitAgentSession *, gchar *text, gpointer user_data) noexcept {
        guard_callback("show_error", [&] { static_cast<Impl *>(user_data)->set_info(text, true); });
    }

    static void show_info(PolkitAgentSession *, gchar *text, gpointer user_data) noexcept {
        guard_callback("show_info", [&] { static_cast<Impl *>(user_data)->set_info(text, false); });
    }

    void changed() {
        if (on_change) {
            on_change();
        }
    }

    void set_info(const gchar *text, bool error) {
        info = text != nullptr ? text : "";
        info_error = error;
        changed();
    }

    void clear_conversation() {
        response_required = false;
        info.clear();
        info_error = false;
    }

    void stop_session() {
        if (session != nullptr) {
            g_signal_handlers_disconnect_by_data(session, this);
            g_object_unref(session);
            session = nullptr;
        }
    }

    void clear_pending(const char *reason, bool silent = false) {
        stop_session();
        if (pending != nullptr) {
            pending->cancel(reason);
        }
        pending.reset();
        cancelling = false;
        clear_conversation();
        if (!silent) {
            changed();
        }
    }

    void begin(std::unique_ptr<AuthRequest> request) {
        if (pending != nullptr) {
            clear_pending("Replaced by a newer authentication request", true);
        }
        if (request->identities.empty()) {
            log::error("polkit: request {} has no identities", request->action_id);
            request->cancel("Authentication request has no identities");
            return;
        }
        pending = std::move(request);
        clear_conversation();
        if (!start_session()) {
            log::error("polkit: cannot start a session for {}", pending->action_id);
            clear_pending("Failed to start authentication session");
            return;
        }
        changed();
    }

    PolkitIdentity *choose_identity() const {
        auto uid = static_cast<gint>(geteuid());
        PolkitIdentity *first_user = nullptr;
        for (const IdentityRef &identity : pending->identities) {
            if (!POLKIT_IS_UNIX_USER(identity.get())) {
                continue;
            }
            if (polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity.get())) == uid) {
                return identity.get();
            }
            if (first_user == nullptr) {
                first_user = identity.get();
            }
        }
        return first_user;
    }

    bool start_session() {
        stop_session();
        PolkitIdentity *identity = choose_identity();
        if (identity == nullptr) {
            log::error("polkit: {} has no unix-user identity", pending->action_id);
            return false;
        }
        session = polkit_agent_session_new(identity, pending->cookie.c_str());
        if (session == nullptr) {
            return false;
        }
        log::info("polkit: authenticating {} as {}", pending->action_id, identity_name(identity));
        g_signal_connect(G_OBJECT(session), "completed", G_CALLBACK(&Impl::completed), this);
        g_signal_connect(G_OBJECT(session), "request", G_CALLBACK(&Impl::request), this);
        g_signal_connect(G_OBJECT(session), "show-error", G_CALLBACK(&Impl::show_error), this);
        g_signal_connect(G_OBJECT(session), "show-info", G_CALLBACK(&Impl::show_info), this);
        polkit_agent_session_initiate(session);
        return true;
    }

    void on_completed(bool gained) {
        if (pending == nullptr) {
            return;
        }
        if (gained) {
            log::info("polkit: {} authorized", pending->action_id);
            pending->complete();
            pending.reset();
            stop_session();
            clear_conversation();
            changed();
            return;
        }
        if (cancelling) {
            clear_pending("Authentication request cancelled");
            return;
        }
        response_required = false;
        info = "Incorrect password, try again";
        info_error = true;
        changed();
        if (!start_session()) {
            clear_pending("Failed to restart authentication session");
        }
    }

    void cancel_from_authority(AuthRequest *request) {
        if (pending == nullptr || pending.get() != request) {
            return;
        }
        cancelling = true;
        clear_pending("Authentication request cancelled by polkit");
    }

    void respond(std::string &response) {
        if (pending != nullptr && session != nullptr && response_required && !response.empty()) {
            polkit_agent_session_response(session, response.c_str());
            response_required = false;
            info.clear();
            info_error = false;
        }
        explicit_bzero(response.data(), response.size());
        response.clear();
        changed();
    }

    void cancel() {
        if (pending == nullptr) {
            return;
        }
        cancelling = true;
        if (session != nullptr) {
            polkit_agent_session_cancel(session);
        }
        clear_pending("Authentication request cancelled by user");
    }

    int prepare(std::vector<pollfd> &fds) {
        poll_fds.clear();
        if (!g_main_context_acquire(context)) {
            return -1;
        }
        gboolean ready = g_main_context_prepare(context, &max_priority);
        gint timeout = -1;
        gint count = 0;
        while ((count = g_main_context_query(context, max_priority, &timeout, poll_fds.data(),
                                             static_cast<gint>(poll_fds.size()))) >
               static_cast<gint>(poll_fds.size())) {
            poll_fds.resize(static_cast<std::size_t>(count));
        }
        poll_fds.resize(static_cast<std::size_t>(count));
        g_main_context_release(context);
        for (const GPollFD &fd : poll_fds) {
            fds.push_back({fd.fd, static_cast<short>(fd.events), 0});
        }
        return ready ? 0 : timeout;
    }

    void dispatch(std::span<const pollfd> fds) {
        if (!g_main_context_acquire(context)) {
            return;
        }
        for (std::size_t i = 0; i < poll_fds.size(); ++i) {
            poll_fds[i].revents = i < fds.size() ? static_cast<gushort>(fds[i].revents) : 0;
        }
        if (g_main_context_check(context, max_priority, poll_fds.data(),
                                 static_cast<gint>(poll_fds.size()))) {
            g_main_context_dispatch(context);
        }
        g_main_context_release(context);
    }
};

PolkitService::PolkitService(EventLoop &loop, std::function<void()> on_change)
    : loop_(loop), impl_(std::make_unique<Impl>(std::move(on_change))) {
    source_ = loop_.add_poll_source([this](std::vector<pollfd> &fds) { return impl_->prepare(fds); },
                                    [this](std::span<const pollfd> fds) { impl_->dispatch(fds); });
    impl_->start();
}

PolkitService::~PolkitService() { loop_.remove_poll_source(source_); }

bool PolkitService::pending() const { return impl_->pending != nullptr; }

std::string PolkitService::message() const {
    if (impl_->pending == nullptr) {
        return "";
    }
    return impl_->pending->message.empty() ? impl_->pending->action_id : impl_->pending->message;
}

bool PolkitService::response_required() const { return impl_->response_required; }

std::string PolkitService::info() const { return impl_->info; }

bool PolkitService::info_is_error() const { return impl_->info_error; }

void PolkitService::respond(std::string &response) { impl_->respond(response); }

void PolkitService::cancel() { impl_->cancel(); }

} // namespace astralia
