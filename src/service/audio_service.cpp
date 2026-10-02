#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <pipewire/extensions/metadata.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/node/keys.h>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/param/route.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/parser.h>
#include <spa/pod/vararg.h>
#include <spa/utils/keys.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/log.h"

#include "service/audio_service.h"

namespace astralia {

namespace {

// PipeWire limits
constexpr std::size_t max_channels = 64;

struct Device {
    pw_proxy *proxy = nullptr;
    spa_hook listener{};
    std::unordered_map<int32_t, int32_t> route_index;
};

std::string json_name(const char *value) {
    if (value == nullptr) {
        return {};
    }
    std::string_view json(value);
    std::size_t key = json.find("\"name\"");
    if (key == std::string_view::npos) {
        return {};
    }
    std::size_t start = json.find('"', json.find(':', key) + 1);
    if (start == std::string_view::npos) {
        return {};
    }
    std::size_t end = json.find('"', start + 1);
    if (end == std::string_view::npos) {
        return {};
    }
    return std::string(json.substr(start + 1, end - start - 1));
}

const char *lookup(const spa_dict *props, const char *key) {
    return props != nullptr ? spa_dict_lookup(props, key) : nullptr;
}

} // namespace

int audio_percent(std::span<const float> channel_volumes) {
    if (channel_volumes.empty()) {
        return 0;
    }
    double total = 0.0;
    for (float volume : channel_volumes) {
        total += std::cbrt(volume);
    }
    return static_cast<int>(std::lround(total / channel_volumes.size() * 100.0));
}

struct AudioService::Impl {
    struct Node {
        Impl *impl = nullptr;
        uint32_t id = 0;
        pw_proxy *proxy = nullptr;
        spa_hook listener{};
        std::string name;
        bool is_sink = false;
        uint32_t channels = 2;
        int percent = 0;
        bool muted = false;
        uint32_t device_id = 0;
        int32_t card_profile_device = -1;
    };

    pw_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    spa_hook registry_listener{};
    pw_proxy *metadata = nullptr;
    spa_hook metadata_listener{};
    std::unordered_map<uint32_t, Node> nodes;
    std::unordered_map<uint32_t, Device> devices;
    std::string sink_name;
    std::string source_name;
    uint32_t sink_id = 0;
    uint32_t source_id = 0;
    bool sink_changed = false;
    bool source_changed = false;

    void mark(uint32_t id) {
        if (id == 0) {
            return;
        }
        if (id == sink_id) {
            sink_changed = true;
        } else if (id == source_id) {
            source_changed = true;
        }
    }

    AudioLevel level(uint32_t id) const {
        auto it = nodes.find(id);
        if (it == nodes.end()) {
            return {};
        }
        return {it->second.percent, it->second.muted, true};
    }

    static void node_param(void *data, int, uint32_t id, uint32_t index, uint32_t,
                           const spa_pod *param) {
        if (id != SPA_PARAM_Props || index != 0 || param == nullptr) {
            return;
        }
        auto *node = static_cast<Node *>(data);
        int percent = node->percent;
        bool muted = node->muted;
        if (const spa_pod_prop *prop = spa_pod_find_prop(param, nullptr, SPA_PROP_channelVolumes)) {
            std::array<float, max_channels> volumes{};
            uint32_t count = spa_pod_copy_array(&prop->value, SPA_TYPE_Float, volumes.data(),
                                                static_cast<uint32_t>(volumes.size()));
            if (count > 0) {
                percent = audio_percent(std::span(volumes.data(), count));
                node->channels = count;
            }
        }
        if (const spa_pod_prop *prop = spa_pod_find_prop(param, nullptr, SPA_PROP_mute)) {
            spa_pod_get_bool(&prop->value, &muted);
        }
        if (percent == node->percent && muted == node->muted) {
            return;
        }
        node->percent = percent;
        node->muted = muted;
        node->impl->mark(node->id);
    }

    static void node_info(void *data, const pw_node_info *info) {
        auto *node = static_cast<Node *>(data);
        if ((info->change_mask & PW_NODE_CHANGE_MASK_PROPS) != 0) {
            if (const char *device = lookup(info->props, "card.profile.device")) {
                node->card_profile_device = std::atoi(device);
            }
        }
        if ((info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) == 0) {
            return;
        }
        for (uint32_t i = 0; i < info->n_params; ++i) {
            const spa_param_info &param = info->params[i];
            if (param.id == SPA_PARAM_Props &&
                (param.flags & SPA_PARAM_INFO_READWRITE) == SPA_PARAM_INFO_READWRITE) {
                pw_node_enum_params(reinterpret_cast<pw_node *>(node->proxy), 0, SPA_PARAM_Props, 0,
                                    UINT32_MAX, nullptr);
            }
        }
    }

    static void device_param(void *data, int, uint32_t id, uint32_t, uint32_t,
                             const spa_pod *param) {
        if (id != SPA_PARAM_Route || param == nullptr) {
            return;
        }
        spa_pod_parser parser;
        spa_pod_parser_pod(&parser, param);
        int32_t device = 0;
        int32_t index = 0;
        uint32_t route = SPA_PARAM_Route;
        if (spa_pod_parser_get_object(&parser, SPA_TYPE_OBJECT_ParamRoute, &route,
                                      SPA_PARAM_ROUTE_device, SPA_POD_Int(&device),
                                      SPA_PARAM_ROUTE_index, SPA_POD_Int(&index)) < 0) {
            return;
        }
        static_cast<Device *>(data)->route_index[device] = index;
    }

    static void device_info(void *data, const pw_device_info *info) {
        if ((info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) == 0) {
            return;
        }
        auto *device = static_cast<Device *>(data);
        for (uint32_t i = 0; i < info->n_params; ++i) {
            const spa_param_info &param = info->params[i];
            if (param.id == SPA_PARAM_Route &&
                (param.flags & SPA_PARAM_INFO_READWRITE) == SPA_PARAM_INFO_READWRITE) {
                pw_device_enum_params(reinterpret_cast<pw_device *>(device->proxy), 0,
                                      SPA_PARAM_Route, 0, UINT32_MAX, nullptr);
            }
        }
    }

    static int metadata_property(void *data, uint32_t, const char *key, const char *,
                                 const char *value) {
        if (key == nullptr) {
            return 0;
        }
        bool sink = std::strcmp(key, "default.audio.sink") == 0;
        if (!sink && std::strcmp(key, "default.audio.source") != 0) {
            return 0;
        }
        auto *impl = static_cast<Impl *>(data);
        std::string name = json_name(value);
        uint32_t resolved = 0;
        for (const auto &[id, node] : impl->nodes) {
            if (node.name == name && node.is_sink == sink) {
                resolved = id;
                break;
            }
        }
        if (sink) {
            impl->sink_name = name;
            impl->sink_id = resolved;
            impl->sink_changed = true;
        } else {
            impl->source_name = name;
            impl->source_id = resolved;
            impl->source_changed = true;
        }
        return 0;
    }

    static void registry_global(void *data, uint32_t id, uint32_t, const char *type, uint32_t,
                                const spa_dict *props) {
        auto *impl = static_cast<Impl *>(data);
        if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
            const char *media_class = lookup(props, SPA_KEY_MEDIA_CLASS);
            bool sink = media_class != nullptr && std::strcmp(media_class, "Audio/Sink") == 0;
            bool source = media_class != nullptr && std::strcmp(media_class, "Audio/Source") == 0;
            if (!sink && !source) {
                return;
            }
            const char *name = lookup(props, SPA_KEY_NODE_NAME);
            const char *device_id = lookup(props, PW_KEY_DEVICE_ID);
            const char *profile_device = lookup(props, "card.profile.device");
            Node &node = impl->nodes[id];
            node.impl = impl;
            node.id = id;
            node.is_sink = sink;
            node.name = name != nullptr ? name : "";
            node.device_id = device_id != nullptr ? std::strtoul(device_id, nullptr, 10) : 0;
            node.card_profile_device = profile_device != nullptr ? std::atoi(profile_device) : -1;
            node.proxy = static_cast<pw_proxy *>(
                pw_registry_bind(impl->registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
            pw_node_add_listener(reinterpret_cast<pw_node *>(node.proxy), &node.listener,
                                 &node_events, &node);
            std::array<uint32_t, 1> params{SPA_PARAM_Props};
            pw_node_subscribe_params(reinterpret_cast<pw_node *>(node.proxy), params.data(),
                                     params.size());
            if (sink && node.name == impl->sink_name) {
                impl->sink_id = id;
            }
            if (source && node.name == impl->source_name) {
                impl->source_id = id;
            }
        } else if (std::strcmp(type, PW_TYPE_INTERFACE_Device) == 0) {
            Device &device = impl->devices[id];
            device.proxy = static_cast<pw_proxy *>(pw_registry_bind(
                impl->registry, id, PW_TYPE_INTERFACE_Device, PW_VERSION_DEVICE, 0));
            pw_device_add_listener(reinterpret_cast<pw_device *>(device.proxy), &device.listener,
                                   &device_events, &device);
        } else if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && impl->metadata == nullptr) {
            const char *name = lookup(props, PW_KEY_METADATA_NAME);
            if (name == nullptr || std::strcmp(name, "default") != 0) {
                return;
            }
            impl->metadata = static_cast<pw_proxy *>(pw_registry_bind(
                impl->registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
            pw_metadata_add_listener(reinterpret_cast<pw_metadata *>(impl->metadata),
                                     &impl->metadata_listener, &metadata_events, impl);
        }
    }

    static void registry_global_remove(void *data, uint32_t id) {
        auto *impl = static_cast<Impl *>(data);
        if (auto it = impl->devices.find(id); it != impl->devices.end()) {
            spa_hook_remove(&it->second.listener);
            pw_proxy_destroy(it->second.proxy);
            impl->devices.erase(it);
        }
        auto it = impl->nodes.find(id);
        if (it == impl->nodes.end()) {
            return;
        }
        if (impl->sink_id == id) {
            impl->sink_id = 0;
            impl->sink_changed = true;
        }
        if (impl->source_id == id) {
            impl->source_id = 0;
            impl->source_changed = true;
        }
        spa_hook_remove(&it->second.listener);
        pw_proxy_destroy(it->second.proxy);
        impl->nodes.erase(it);
    }

    static constexpr pw_node_events node_events = {
        .version = PW_VERSION_NODE_EVENTS,
        .info = node_info,
        .param = node_param,
    };
    static constexpr pw_device_events device_events = {
        .version = PW_VERSION_DEVICE_EVENTS,
        .info = device_info,
        .param = device_param,
    };
    static constexpr pw_metadata_events metadata_events = {
        .version = PW_VERSION_METADATA_EVENTS,
        .property = metadata_property,
    };
    static constexpr pw_registry_events registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = registry_global,
        .global_remove = registry_global_remove,
    };

    bool connect() {
        pw_init(nullptr, nullptr);
        loop = pw_loop_new(nullptr);
        if (loop == nullptr) {
            log::error("audio: cannot create PipeWire loop");
            return false;
        }
        pw_loop_enter(loop);
        context = pw_context_new(loop, nullptr, 0);
        if (context == nullptr) {
            log::error("audio: cannot create PipeWire context");
            return false;
        }
        core = pw_context_connect(context, nullptr, 0);
        if (core == nullptr) {
            log::error("audio: cannot connect to PipeWire");
            return false;
        }
        registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
        pw_registry_add_listener(registry, &registry_listener, &registry_events, this);
        return true;
    }

    void set_volume(uint32_t id, int percent) {
        auto it = nodes.find(id);
        if (it == nodes.end()) {
            return;
        }
        Node &node = it->second;
        float level = static_cast<float>(percent) / 100.0f;
        std::vector<float> volumes(node.channels, level * level * level);
        std::array<uint8_t, 1024> buffer{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
        auto *props = static_cast<spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props, SPA_PROP_channelVolumes,
            SPA_POD_Array(sizeof(float), SPA_TYPE_Float, volumes.size(), volumes.data())));
        set_props(node, builder, props);
    }

    void set_mute(uint32_t id, bool muted) {
        auto it = nodes.find(id);
        if (it == nodes.end()) {
            return;
        }
        std::array<uint8_t, 1024> buffer{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
        auto *props = static_cast<spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props, SPA_PROP_mute,
            SPA_POD_Bool(muted)));
        set_props(it->second, builder, props);
    }

    void set_props(Node &node, spa_pod_builder &builder, spa_pod *props) {
        auto device = devices.find(node.device_id);
        if (node.card_profile_device >= 0 && device != devices.end()) {
            auto route = device->second.route_index.find(node.card_profile_device);
            if (route != device->second.route_index.end()) {
                auto *param = static_cast<spa_pod *>(spa_pod_builder_add_object(
                    &builder, SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route, SPA_PARAM_ROUTE_device,
                    SPA_POD_Int(node.card_profile_device), SPA_PARAM_ROUTE_index,
                    SPA_POD_Int(route->second), SPA_PARAM_ROUTE_props, SPA_POD_PodObject(props),
                    SPA_PARAM_ROUTE_save, SPA_POD_Bool(true)));
                pw_device_set_param(reinterpret_cast<pw_device *>(device->second.proxy),
                                    SPA_PARAM_Route, 0, param);
                return;
            }
        }
        pw_node_set_param(reinterpret_cast<pw_node *>(node.proxy), SPA_PARAM_Props, 0, props);
    }

    ~Impl() {
        for (auto &[id, node] : nodes) {
            spa_hook_remove(&node.listener);
            pw_proxy_destroy(node.proxy);
        }
        for (auto &[id, device] : devices) {
            spa_hook_remove(&device.listener);
            pw_proxy_destroy(device.proxy);
        }
        if (metadata != nullptr) {
            spa_hook_remove(&metadata_listener);
            pw_proxy_destroy(metadata);
        }
        if (registry != nullptr) {
            spa_hook_remove(&registry_listener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
        }
        if (core != nullptr) {
            pw_core_disconnect(core);
        }
        if (context != nullptr) {
            pw_context_destroy(context);
        }
        if (loop != nullptr) {
            pw_loop_leave(loop);
            pw_loop_destroy(loop);
        }
        pw_deinit();
    }
};

AudioService::AudioService(EventLoop &loop) : loop_(loop), impl_(std::make_unique<Impl>()) {
    if (!impl_->connect()) {
        return;
    }
    loop_.on_fd(pw_loop_get_fd(impl_->loop), [this] {
        pw_loop_iterate(impl_->loop, 0);
        if (std::exchange(impl_->sink_changed, false)) {
            changed.emit(AudioKind::sink);
        }
        if (std::exchange(impl_->source_changed, false)) {
            changed.emit(AudioKind::source);
        }
    });
}

AudioService::~AudioService() {
    if (impl_->core != nullptr) {
        loop_.remove_fd(pw_loop_get_fd(impl_->loop));
    }
}

AudioLevel AudioService::sink() const { return impl_->level(impl_->sink_id); }

AudioLevel AudioService::source() const { return impl_->level(impl_->source_id); }

void AudioService::set_sink_volume(int percent) {
    impl_->set_volume(impl_->sink_id, percent);
}

void AudioService::set_sink_mute(bool muted) {
    impl_->set_mute(impl_->sink_id, muted);
}

} // namespace astralia
