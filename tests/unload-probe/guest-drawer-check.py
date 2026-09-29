"""Private-bus guest protocol check; requires Virtual-0 tablet fixture."""
import json
import os
import time
import dbus

assert os.environ["XDG_RUNTIME_DIR"].startswith("/tmp/kadunce-unload-")
bus = dbus.SessionBus(private=True)
guest = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "studio.warbler.Kadunce")
reply = json.loads(guest.beginLauncherGuest(bus.get_unique_name()))
assert reply["accepted"] and reply["presentationCapability"] == 1
assert reply["active"]["width"] > reply["card"]["width"]
other = dbus.SessionBus(private=True)
stranger = dbus.Interface(other.get_object("org.kde.KWin", "/Kadunce"), "studio.warbler.Kadunce")
assert not stranger.setLauncherGuestExpanded(True)
assert guest.setLauncherGuestExpanded(True)
time.sleep(.10)
assert 0 < json.loads(guest.nativeCarryState())["guestNeighborOpacity"] < 1
time.sleep(.18)
assert json.loads(guest.nativeCarryState())["guestNeighborOpacity"] == 0
assert not guest.finishLauncherGuest(500)
assert guest.setLauncherGuestExpanded(False)
time.sleep(.10)
assert 0 < json.loads(guest.nativeCarryState())["guestNeighborOpacity"] < 1
time.sleep(.18)
assert json.loads(guest.nativeCarryState())["guestNeighborOpacity"] == 1
print("PASS: neighbor departure and return settle on a bounded shared clock")
assert guest.setLauncherGuestExpanded(True)
guest.endLauncherGuest()
assert not guest.setLauncherGuestExpanded(True)
print("PASS: expansion owner, compact/Active bounds, navigation suppression and closed lease")
reply = json.loads(guest.beginLauncherGuest(bus.get_unique_name()))
assert reply["accepted"] and guest.setLauncherGuestExpanded(True)
bus.close()
time.sleep(.2)
assert not stranger.setLauncherGuestExpanded(True)
effects = dbus.Interface(other.get_object("org.kde.KWin", "/Effects"), "org.kde.kwin.Effects")
effects.unloadEffect("kwin4_effect_kadunce")
print("PASS: expanded guest owner loss and effect unload")
