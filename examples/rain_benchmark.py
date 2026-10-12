"""Bounded per-LED rain benchmark for every Direct-capable OpenRGB SDK device.

By default the AULA keyboard is excluded: its cmd-9 color table is persistent.
Use --include-aula for a short, capped run, after exposing plugin controllers
through Settings > SDK Server > All Controllers in the *server* OpenRGB instance.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import statistics
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
AULA_NAME = "AULA WIN 68 HE"


@dataclass(frozen=True)
class Drop:
    x: float
    y: float
    speed: float


def positions(device) -> list[tuple[float, float]]:
    if device.name == AULA_NAME and len(device.leds) == 68:
        layout = json.loads((HERE / "win68-led-layout.json").read_text(encoding="utf-8"))
        if [led.name for led in device.leds] == [item["name"] for item in layout]:
            return [(item["x"] / 615, item["y"] / 202) for item in layout]
        raise ValueError("AULA LED names/order differ from the bundled WIN68 layout")
    count = len(device.leds)
    return [(i / max(1, count - 1), 0.5) for i in range(count)]


def rain_colors(points: list[tuple[float, float]], drops: list[Drop], t: float):
    from openrgb.utils import RGBColor

    colors = []
    for x, y in points:
        glow = 0.0
        for drop in drops:
            dy = y - ((drop.y + drop.speed * t) % 1.7 - 0.35)
            dx = abs(x - drop.x)
            # A bright falling head and a softer trail above it.
            head = max(0.0, 1.0 - math.hypot(dx / 0.10, dy / 0.20))
            trail = max(0.0, 1.0 - dx / 0.055) * max(0.0, 1.0 + dy / 0.32) if dy <= 0 else 0.0
            glow = max(glow, head + 0.25 * trail)
        intensity = min(1.0, glow)
        colors.append(RGBColor(0, round(8 + 115 * intensity), round(18 + 225 * intensity)))
    return colors


def args_parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=6742)
    p.add_argument("--seconds", type=float, default=10.0)
    p.add_argument("--fps", type=float, default=10.0, help="overall target, 1..60")
    p.add_argument("--include-aula", action="store_true", help="include persistent WIN68 palette writes")
    p.add_argument("--aula-fps", type=float, help="optional WIN68 rate cap; defaults to --fps")
    p.add_argument("--dry-run", action="store_true", help="generate frames and count them without any writes")
    p.add_argument("--list", action="store_true", help="show SDK device/LED names without changing lighting")
    p.add_argument("--leave-effect", action="store_true", help="skip restoring original modes and colors")
    p.add_argument("--fast", action="store_true", help="skip SDK state read-back after each frame")
    p.add_argument("--seed", type=int, default=68)
    return p


def percentile(values: list[float], fraction: float) -> float:
    return sorted(values)[min(len(values) - 1, math.ceil(len(values) * fraction) - 1)] if values else 0.0


def main() -> int:
    a = args_parser().parse_args()
    if a.aula_fps is None:
        a.aula_fps = a.fps
    if not (1 <= a.fps <= 60 and 0.2 <= a.aula_fps <= 60 and 0 < a.seconds <= 60):
        raise SystemExit("Limits: 1..60 fps, 0.2..60 AULA fps, greater than 0 up to 60 seconds")
    from openrgb import OpenRGBClient

    try:
        client = OpenRGBClient(a.host, a.port, name="AULA WIN68 rain benchmark")
    except Exception as exc:
        raise SystemExit(f"Cannot connect to OpenRGB SDK at {a.host}:{a.port}: {exc}") from exc

    for device in client.devices:
        print(f"{device.name}: {len(device.leds)} LEDs, modes: {', '.join(m.name for m in device.modes)}")
        if a.list:
            print("  " + ", ".join(led.name for led in device.leds))
    if not any(d.name == AULA_NAME for d in client.devices):
        print("WARNING: SDK server does not expose WIN68. Its process must load the plugin and include All Controllers.")
    if a.list:
        return 0
    if a.include_aula and not any(d.name == AULA_NAME for d in client.devices):
        raise SystemExit("WIN68 missing from SDK. The server process must load the AULA plugin and enable 'All Controllers'; if another OpenRGB owns port 6742, use its server port or --port.")

    selected = []
    for device in client.devices:
        if not device.leds or not any(m.name.lower() == "direct" for m in device.modes):
            print(f"SKIP {device.name}: no addressable Direct mode")
            continue
        if device.name == AULA_NAME and not a.include_aula:
            print("SKIP WIN68: persistent cmd-9 palette (use --include-aula for a capped test)")
            continue
        selected.append((device, positions(device)))
    if not selected:
        raise SystemExit("No Direct-capable devices available in this SDK server")

    rng = random.Random(a.seed)
    drops = [Drop(rng.random(), rng.random() * 1.7, rng.uniform(0.18, 0.43)) for _ in range(16)]
    snapshots = {}
    results = {d.name: {"leds": len(d.leds), "frames": 0, "errors": [], "latencies_ms": []} for d, _ in selected}
    if not a.dry_run:
        for device, _ in selected:
            try:
                snapshots[device.name] = (device.modes[device.active_mode].name, list(device.colors))
                device.set_mode("Direct")
            except Exception as exc:
                results[device.name]["errors"].append(f"set_mode: {exc}")
        selected = [(d, p) for d, p in selected if not results[d.name]["errors"]]
    if not selected:
        raise SystemExit("No device could enter Direct mode")

    started = time.monotonic()
    last_sent = {d.name: float("-inf") for d, _ in selected}
    tick = 0
    try:
        while True:
            now = time.monotonic()
            elapsed = now - started
            if elapsed >= a.seconds:
                break
            for device, points in selected:
                if device.name == AULA_NAME and elapsed - last_sent[device.name] < 1 / a.aula_fps:
                    continue
                frame = rain_colors(points, drops, elapsed)
                send_start = time.monotonic()
                try:
                    if not a.dry_run:
                        device.set_colors(frame, fast=a.fast)
                    stats = results[device.name]
                    stats["frames"] += 1
                    stats["latencies_ms"].append(round((time.monotonic() - send_start) * 1000, 2))
                    last_sent[device.name] = elapsed
                except Exception as exc:
                    results[device.name]["errors"].append(str(exc))
                    selected = [(d, p) for d, p in selected if d.name != device.name]
            if not selected:
                break
            tick += 1
            remaining = started + tick / a.fps - time.monotonic()
            if remaining > 0:
                time.sleep(remaining)
    except KeyboardInterrupt:
        print("Interrupted; restoring prior settings.")
    finally:
        if not a.dry_run and not a.leave_effect:
            for device in client.devices:
                if device.name not in snapshots:
                    continue
                mode, colors = snapshots[device.name]
                try:
                    if mode.lower() == "direct":
                        device.set_colors(colors)
                    else:
                        device.set_mode(mode)
                except Exception as exc:
                    results[device.name]["errors"].append(f"restore: {exc}")

    duration = round(time.monotonic() - started, 3)
    for stats in results.values():
        times = stats.pop("latencies_ms")
        stats["achieved_fps"] = round(stats["frames"] / max(duration, .001), 2)
        stats["avg_ms"] = round(statistics.mean(times), 2) if times else 0.0
        stats["p95_ms"] = percentile(times, .95)
    report = {"timestamp": datetime.now().astimezone().isoformat(), "duration_s": duration,
              "target_fps": a.fps, "aula_fps_limit": a.aula_fps, "dry_run": a.dry_run,
              "fast_sdk": a.fast, "note": "SDK completion is not optical/hardware verification",
              "devices": results}
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if not a.dry_run:
        reports = HERE.parent / "reports"
        reports.mkdir(exist_ok=True)
        output = reports / f"rain-{datetime.now():%Y%m%d-%H%M%S}.json"
        output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"Report: {output}")
    return 1 if any(r["errors"] for r in results.values()) else 0


if __name__ == "__main__":
    raise SystemExit(main())
