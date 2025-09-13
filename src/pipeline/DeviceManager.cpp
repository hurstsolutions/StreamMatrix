#include "DeviceManager.h"
#include <gst/gstdevice.h>
#include <gst/gstdevicemonitor.h>

DeviceManager::DeviceManager() {}

DeviceManager::~DeviceManager() {
  clearList(video_);
  clearList(audio_);
}

void DeviceManager::clearList(std::vector<DeviceInfo>& list) {
  for (auto& d : list) {
    if (d.device) g_object_unref(d.device);
  }
  list.clear();
}

QString DeviceManager::getDisplayName(GstDevice* dev) {
  const gchar* name = gst_device_get_display_name(dev);
  if (name) return QString::fromUtf8(name);
  // fallback to "device.api + device.path"
  GstStructure* props = gst_device_get_properties(dev);
  QString n = "Unknown";
  if (props) {
    const gchar* api = nullptr;
    gst_structure_get(props, "device.api", G_TYPE_STRING, &api, nullptr);
    if (api) n = QString::fromUtf8(api);
    gst_structure_free(props);
  }
  return n;
}

QString DeviceManager::getApi(GstDevice* dev) {
  GstStructure* props = gst_device_get_properties(dev);
  QString apiStr = "unknown";
  if (props) {
    const gchar* api = nullptr;
    gst_structure_get(props, "device.api", G_TYPE_STRING, &api, nullptr);
    if (api) apiStr = QString::fromUtf8(api);
    gst_structure_free(props);
  }
  return apiStr;
}

std::vector<DeviceInfo> DeviceManager::enumerateClass(const char* klass) {
  std::vector<DeviceInfo> out;

  GstDeviceMonitor* mon = gst_device_monitor_new();
  gst_device_monitor_add_filter(mon, klass, nullptr);
  gst_device_monitor_start(mon);

  GList* devs = gst_device_monitor_get_devices(mon);
  for (GList* l = devs; l != nullptr; l = l->next) {
    auto* dev = GST_DEVICE(l->data);
    g_object_ref(dev);
    DeviceInfo info;
    info.device = dev;
    info.displayName = getDisplayName(dev);
    info.api = getApi(dev);
    out.emplace_back(std::move(info));
  }
  gst_device_monitor_stop(mon);
  g_list_free(devs);
  g_object_unref(mon);

  return out;
}

void DeviceManager::refresh() {
  clearList(video_);
  clearList(audio_);
  video_ = enumerateClass("Video/Source");
  audio_ = enumerateClass("Audio/Source");
}