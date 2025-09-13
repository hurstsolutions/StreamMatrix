#pragma once
#include <QString>
#include <vector>
#include <gst/gst.h>

struct DeviceInfo {
  QString displayName;
  QString api;
  GstDevice* device{nullptr};  // owned (ref'd)
};

class DeviceManager {
public:
  DeviceManager();
  ~DeviceManager();

  void refresh();

  int videoCount() const { return static_cast<int>(video_.size()); }
  int audioCount() const { return static_cast<int>(audio_.size()); }

  const DeviceInfo& video(int idx) const { return video_.at(idx); }
  const DeviceInfo& audio(int idx) const { return audio_.at(idx); }

private:
  static std::vector<DeviceInfo> enumerateClass(const char* klass);

  static QString getDisplayName(GstDevice* dev);
  static QString getApi(GstDevice* dev);

  static void clearList(std::vector<DeviceInfo>& list);

  std::vector<DeviceInfo> video_;
  std::vector<DeviceInfo> audio_;
};