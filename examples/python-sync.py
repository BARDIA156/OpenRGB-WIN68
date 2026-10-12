"""Run after enabling OpenRGB's SDK server and selecting the Direct mode."""
from openrgb import OpenRGBClient
from openrgb.utils import RGBColor

client = OpenRGBClient(name="AULA WIN68 example")
keyboard = next(d for d in client.devices if d.name == "AULA WIN 68 HE")
keyboard.set_mode("Direct")

# A single-key update. The DLL translates OpenRGB ordinal -> vendor matrix index.
escape = next(led for led in keyboard.leds if led.name == "Escape")
escape.set_color(RGBColor(255, 0, 0))

# One uniform color across the keyboard and other connected devices.
color = RGBColor(0, 120, 255)
for device in client.devices:
    if device.name == keyboard.name:
        device.set_mode("Direct")
    device.set_color(color)
