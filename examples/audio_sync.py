"""Bounded audio-reactive OpenRGB SDK test using stereo WASAPI loopback.

The installed Effects plugin may reject the Windows-reported eight-channel mix
format even when stereo loopback works. This script uses the working stereo
capture path. Stop other effects controlling the same LEDs before running it.
The WIN68 firmware palette is persistent: include it only for a short test.
"""
from __future__ import annotations

import argparse
import colorsys
import time
from array import array

import numpy as np
import pyaudiowpatch as pa
from openrgb import OpenRGBClient
from openrgb.utils import RGBColor

from rain_benchmark import AULA_NAME, positions


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6742)
    parser.add_argument("--seconds", type=float, default=10.0)
    parser.add_argument("--fps", type=float, default=10.0)
    parser.add_argument("--include-aula", action="store_true",
                        help="include persistent WIN68 palette writes at the requested FPS; rapid updates may disrupt typing")
    parser.add_argument("--dry-run", action="store_true", help="capture and render without LED writes")
    parser.add_argument("--list", action="store_true", help="list devices and audio source without LED writes")
    return parser.parse_args()


def loopback_source(audio):
    host = audio.get_host_api_info_by_type(pa.paWASAPI)
    output = audio.get_device_info_by_index(host["defaultOutputDevice"])
    device = next((d for d in audio.get_loopback_device_info_generator()
                   if output["name"] in d["name"]), None)
    if device is None:
        raise RuntimeError(f"No WASAPI loopback for default playback: {output['name']}")
    return device


def spectral_levels(pcm, window):
    samples = np.asarray(array("f", pcm), dtype=np.float32)
    mono = samples.reshape(-1, 2).mean(axis=1)
    spectrum = np.abs(np.fft.rfft(mono * window))
    # 16 log-spaced bands make bass and voice more visible than linear bins.
    edges = np.geomspace(2, len(spectrum), 17).astype(int)
    return np.array([np.mean(spectrum[edges[i]:max(edges[i] + 1, edges[i + 1])])
                     for i in range(16)], dtype=float)


def colors_for(points, levels):
    result = []
    for x, _y in points:
        band = min(15, int(x * 16))
        value = max(0.0, min(1.0, float(levels[band])))
        rgb = colorsys.hsv_to_rgb(0.62 - 0.54 * x, 1.0, value)
        result.append(RGBColor(*(round(channel * 255) for channel in rgb)))
    return result


def main():
    args = parse_args()
    if not 0.5 <= args.seconds <= 30 or not 1 <= args.fps <= 60:
        raise SystemExit("Limits: 0.5..30 seconds and 1..60 fps")
    with pa.PyAudio() as audio:
        source = loopback_source(audio)
        print("Playback loopback:", source["name"], "(stereo, 48000 Hz if supported)")
        try:
            client = OpenRGBClient(args.host, args.port, name="AULA stereo audio sync test")
        except Exception as exc:
            raise SystemExit(f"OpenRGB SDK connection failed at {args.host}:{args.port}: {exc}") from exc
        devices = client.devices
        for device in devices:
            print(f"SDK device: {device.name}, {len(device.leds)} LEDs")
        if not any(d.name == AULA_NAME for d in devices):
            print("WIN68 is absent from this SDK server. Enable All Controllers in the OpenRGB process that owns the SDK port.")
        if args.list:
            return 0
        if args.include_aula and not any(d.name == AULA_NAME for d in devices):
            raise SystemExit("--include-aula requested, but the SDK server does not expose WIN68")

        selected = []
        for device in devices:
            if not device.leds or not any(m.name.lower() == "direct" for m in device.modes):
                continue
            if device.name == AULA_NAME and not args.include_aula:
                continue
            selected.append((device, positions(device)))
        if not selected:
            raise SystemExit("No Direct-capable SDK devices were selected")

        snapshots = []
        failures = []
        frames = {device.name: 0 for device, _ in selected}
        rate = int(source["defaultSampleRate"])
        chunk = 512
        window = np.hanning(chunk)
        smoothed = np.zeros(16, dtype=float)
        reference = 0.02
        try:
            if not args.dry_run:
                active = []
                for device, points in selected:
                    try:
                        snapshots.append((device, device.modes[device.active_mode].name, list(device.colors)))
                        device.set_mode("Direct")
                        active.append((device, points))
                    except Exception as exc:
                        failures.append(f"{device.name}: enter Direct: {exc}")
                selected = active
                if not selected:
                    raise RuntimeError("No selected device could enter Direct mode")
            with audio.open(format=pa.paFloat32, channels=2, rate=rate, input=True,
                            input_device_index=source["index"], frames_per_buffer=chunk) as stream:
                start = time.monotonic()
                deadline = start + args.seconds
                tick = 0
                while time.monotonic() < deadline:
                    pcm = stream.read(chunk, exception_on_overflow=False)
                    spectrum = spectral_levels(pcm, window)
                    reference = max(0.02, reference * 0.975, float(spectrum.max()))
                    smoothed = 0.55 * smoothed + 0.45 * np.clip(spectrum / reference, 0, 1)
                    for device, points in selected:
                        try:
                            if not args.dry_run:
                                device.set_colors(colors_for(points, smoothed), fast=True)
                            frames[device.name] += 1
                        except Exception as exc:
                            failures.append(f"{device.name}: frame: {exc}")
                            selected = [(d, p) for d, p in selected if d is not device]
                    if not selected:
                        break
                    tick += 1
                    remaining = start + tick / args.fps - time.monotonic()
                    if remaining > 0:
                        time.sleep(remaining)
        except KeyboardInterrupt:
            print("Stopped by user")
        except (OSError, RuntimeError) as exc:
            failures.append(f"Audio/SDK: {exc}")
        finally:
            if not args.dry_run:
                for device, mode, original_colors in snapshots:
                    try:
                        if mode.lower() == "direct":
                            device.set_colors(original_colors)
                        else:
                            device.set_mode(mode)
                    except Exception as exc:
                        failures.append(f"{device.name}: restore: {exc}")
        print("Frames:", frames)
        print("Errors:", failures)
        return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
