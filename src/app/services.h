#pragma once

#include "core/dbus.h"
#include "core/event_loop.h"
#include "core/x_connection.h"

#include "service/audio_service.h"
#include "service/battery_service.h"
#include "service/bluetooth_service.h"
#include "service/brightness_service.h"
#include "service/i3_service.h"
#include "service/media_service.h"
#include "service/network_service.h"
#include "service/notification_service.h"
#include "service/polkit_service.h"
#include "service/tray_service.h"

namespace astralia {

struct Services {
    Services(XConnection &x, EventLoop &loop);
    Services(const Services &) = delete;
    Services &operator=(const Services &) = delete;

    SystemBus system;
    SystemBus session;
    I3Service i3;
    NetworkService network;
    BluetoothService bluetooth;
    BatteryService battery;
    BrightnessService brightness;
    NotificationService notifications;
    PolkitService polkit;
    AudioService audio;
    MediaService media;
    TrayService tray;
};

} // namespace astralia
