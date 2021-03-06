// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// BluetoothGattDescriptorClient is used to communicate with remote GATT
// characteristic descriptor objects exposed by the Bluetooth daemon.
class CHROMEOS_EXPORT BluetoothGattDescriptorClient : public DBusClient {
 public:
  // Structure of properties associated with GATT descriptors.
  struct Properties : public dbus::PropertySet {
    // The 128-bit characteristic descriptor UUID. [read-only]
    dbus::Property<std::string> uuid;

    // Object path of the GATT characteristic the descriptor belongs to.
    // [read-only]
    dbus::Property<dbus::ObjectPath> characteristic;

    // Raw characteristic descriptor value read from the remote Bluetooth
    // device. Setting the value sends a write request to the remote device.
    // [read-write]
    dbus::Property<std::vector<uint8> > value;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a remote GATT characteristic
  // descriptor.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the GATT descriptor with object path |object_path| is added
    // to the system.
    virtual void GattDescriptorAdded(const dbus::ObjectPath& object_path) {}

    // Called when the GATT descriptor with object path |object_path| is removed
    // from the system.
    virtual void GattDescriptorRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the GATT descriptor with object path |object_path| has a
    // change in the value of the property named |property_name|.
    virtual void GattDescriptorPropertyChanged(
        const dbus::ObjectPath& object_path,
        const std::string& property_name) {}
  };

  virtual ~BluetoothGattDescriptorClient();

  // Adds and removes observers for events on all remote GATT descriptors. Check
  // the |object_path| parameter of observer methods to determine which GATT
  // descriptor is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of GATT descriptor object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetDescriptors() = 0;

  // Obtain the properties for the GATT descriptor with object path
  // |object_path|. Values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothGattDescriptorClient* Create();

 protected:
  BluetoothGattDescriptorClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattDescriptorClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
