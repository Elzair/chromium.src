// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "ui/base/x/x11_util.h"

namespace chromeos {
namespace {

// Checks the |event| and asynchronously sets the XKB layout when necessary.
void HandleHierarchyChangedEvent(
    XIHierarchyEvent* event,
    ObserverList<DeviceHierarchyObserver>* observer_list) {
  if (!(event->flags & (XISlaveAdded | XISlaveRemoved)))
    return;

  bool update_keyboard_status = false;
  for (int i = 0; i < event->num_info; ++i) {
    XIHierarchyInfo* info = &event->info[i];
    if ((info->flags & XISlaveAdded) && (info->use == XIFloatingSlave)) {
      FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                        *observer_list,
                        DeviceAdded(info->deviceid));
      update_keyboard_status = true;
    } else if (info->flags & XISlaveRemoved) {
      // Can't check info->use here; it appears to always be 0.
      FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                        *observer_list,
                        DeviceRemoved(info->deviceid));
    }
  }

  if (update_keyboard_status) {
    chromeos::input_method::InputMethodManager* input_method_manager =
        chromeos::input_method::InputMethodManager::Get();
    chromeos::input_method::XKeyboard* xkeyboard =
        input_method_manager->GetXKeyboard();
    xkeyboard->ReapplyCurrentModifierLockStatus();
    xkeyboard->ReapplyCurrentKeyboardLayout();
  }
}

}  // namespace

// static
XInputHierarchyChangedEventListener*
XInputHierarchyChangedEventListener::GetInstance() {
  return Singleton<XInputHierarchyChangedEventListener>::get();
}

XInputHierarchyChangedEventListener::XInputHierarchyChangedEventListener()
    : stopped_(false) {
  base::MessageLoopForUI::current()->AddObserver(this);
}

XInputHierarchyChangedEventListener::~XInputHierarchyChangedEventListener() {
  Stop();
}

void XInputHierarchyChangedEventListener::Stop() {
  if (stopped_)
    return;

  base::MessageLoopForUI::current()->RemoveObserver(this);
  stopped_ = true;
}

void XInputHierarchyChangedEventListener::AddObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.AddObserver(observer);
}

void XInputHierarchyChangedEventListener::RemoveObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

base::EventStatus XInputHierarchyChangedEventListener::WillProcessEvent(
    const base::NativeEvent& event) {
  // There may be multiple listeners for the XI_HierarchyChanged event. So
  // always return EVENT_CONTINUE to make sure all the listeners receive the
  // event.
  ProcessedXEvent(event);
  return base::EVENT_CONTINUE;
}

void XInputHierarchyChangedEventListener::DidProcessEvent(
    const base::NativeEvent& event) {
}

void XInputHierarchyChangedEventListener::ProcessedXEvent(XEvent* xevent) {
  if (xevent->xcookie.type != GenericEvent)
    return;

  XGenericEventCookie* cookie = &(xevent->xcookie);

  if (cookie->evtype == XI_HierarchyChanged) {
    XIHierarchyEvent* event = static_cast<XIHierarchyEvent*>(cookie->data);
    HandleHierarchyChangedEvent(event, &observer_list_);
    if (event->flags & XIDeviceEnabled || event->flags & XIDeviceDisabled)
      NotifyDeviceHierarchyChanged();
  }
}

void XInputHierarchyChangedEventListener::NotifyDeviceHierarchyChanged() {
  FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                    observer_list_,
                    DeviceHierarchyChanged());
}

}  // namespace chromeos
