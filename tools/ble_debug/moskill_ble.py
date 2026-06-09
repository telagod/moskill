#!/usr/bin/env python3
"""MosKill BLE Debug Tool — scan, connect, monitor kills, read stats, sync time, push OTA."""

import asyncio
import struct
import time
import sys
import argparse
import zlib
from pathlib import Path
from datetime import datetime

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from rich.console import Console
from rich.table import Table
from rich.live import Live
from rich.panel import Panel
from rich.layout import Layout
from rich.text import Text

console = Console()

# 128-bit custom UUIDs: base e3a1XXXX-f5e8-4c8a-9b3d-2c1f7b8a6d50
SVC_MOSKILL     = "e3a11b00-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_KILL_COUNT = "e3a11b01-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_SESSION    = "e3a11b02-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_LIFETIME   = "e3a11b03-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_KILL_LOG   = "e3a11b04-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_ENV        = "e3a11b05-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_CONFIG     = "e3a11b11-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_TIME_SYNC  = "e3a11b12-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_OTA_CTRL   = "e3a11b21-f5e8-4c8a-9b3d-2c1f7b8a6d50"
CHAR_BATTERY    = "00002a19-0000-1000-8000-00805f9b34fb"  # standard BAS

KILL_CLASSES = ["S (Small)", "M (Mosquito)", "L (Large)", "XL (Extra)"]
KILL_EMOJI = ["🪰", "🦟", "🪳", "🦋"]


async def scan_devices(timeout: float = 5.0):
    console.print("[bold cyan]Scanning for MosKill devices...[/]")
    devices = await BleakScanner.discover(timeout=timeout)
    moskill_devices = []
    for d in devices:
        if d.name and "MosKill" in d.name:
            moskill_devices.append(d)
            console.print(f"  [green]Found:[/] {d.name} ({d.address}) RSSI={d.rssi}")
    if not moskill_devices:
        console.print("  [yellow]No MosKill devices found. Showing all BLE devices:[/]")
        for d in sorted(devices, key=lambda x: x.rssi or -100, reverse=True)[:10]:
            name = d.name or "(unknown)"
            console.print(f"  {name:30s} {d.address} RSSI={d.rssi}")
    return moskill_devices


def parse_session_stats(data: bytes) -> dict:
    if len(data) < 36:
        return {"error": f"short data ({len(data)} bytes)"}
    fields = struct.unpack_from("<IIIHHHHHHIhHHH", data[:36])
    return {
        "session_id": fields[0],
        "start_time": datetime.fromtimestamp(fields[1]).strftime("%Y-%m-%d %H:%M:%S") if fields[1] else "N/A",
        "duration_sec": fields[2],
        "kills_total": fields[3],
        "kills_S": fields[4],
        "kills_M": fields[5],
        "kills_L": fields[6],
        "kills_XL": fields[7],
        "max_streak": fields[8],
        "energy_total": fields[9],
        "avg_temp": fields[10] / 10.0,
        "avg_humidity": fields[11] / 10.0,
        "hv_on_time_sec": fields[12],
        "efficiency": fields[13] / 100.0,
    }


def parse_lifetime_stats(data: bytes) -> dict:
    if len(data) < 76:
        return {"error": f"short data ({len(data)} bytes)"}
    base = struct.unpack_from("<IIIIIIIIII", data[:40])
    hourly = struct.unpack_from("24B", data, 40)
    daily = struct.unpack_from("7B", data, 64)
    first_use = struct.unpack_from("<I", data, 72)[0]
    return {
        "total_kills": base[0],
        "kills_S": base[1],
        "kills_M": base[2],
        "kills_L": base[3],
        "kills_XL": base[4],
        "total_sessions": base[5],
        "total_active_sec": base[6],
        "best_streak": base[7],
        "best_session_kills": base[8],
        "kill_rate_per_hour": base[9] / 100.0,
        "hourly_histogram": list(hourly),
        "daily_histogram": list(daily),
        "first_use": datetime.fromtimestamp(first_use).strftime("%Y-%m-%d") if first_use else "N/A",
    }


def parse_kill_log_page(data: bytes) -> list:
    if len(data) < 4:
        return []
    page, total_pages, count = struct.unpack_from("<BBH", data)
    events = []
    offset = 4
    for i in range(count):
        if offset + 15 > len(data):
            break
        ts, kclass, peak, dur, energy, temp, humi = struct.unpack_from("<IBHHIbB", data, offset)
        events.append({
            "time": datetime.fromtimestamp(ts).strftime("%H:%M:%S") if ts else "N/A",
            "class": KILL_CLASSES[kclass] if kclass < 4 else f"?{kclass}",
            "emoji": KILL_EMOJI[kclass] if kclass < 4 else "?",
            "peak_adc": peak,
            "duration_ms": dur,
            "energy": energy,
            "temp": temp,
            "humidity": humi,
        })
        offset += 15
    return events


def render_stats_table(session: dict, lifetime: dict) -> Table:
    table = Table(title="MosKill Stats", show_header=True, border_style="cyan")
    table.add_column("Metric", style="bold")
    table.add_column("Session", justify="right")
    table.add_column("Lifetime", justify="right")

    table.add_row("Total Kills", str(session.get("kills_total", "?")), str(lifetime.get("total_kills", "?")))
    for cls, key in zip(KILL_CLASSES, ["kills_S", "kills_M", "kills_L", "kills_XL"]):
        table.add_row(f"  {cls}", str(session.get(key, "?")), str(lifetime.get(key, "?")))
    table.add_row("Best Streak", str(session.get("max_streak", "?")), str(lifetime.get("best_streak", "?")))
    table.add_row("Active Time", f"{session.get('duration_sec', 0)}s",
                  f"{lifetime.get('total_active_sec', 0) // 3600}h")
    table.add_row("Kill Rate/hr", "-", f"{lifetime.get('kill_rate_per_hour', 0):.1f}")
    table.add_row("Sessions", "-", str(lifetime.get("total_sessions", "?")))
    return table


def render_histogram(hourly: list) -> str:
    if not hourly or max(hourly) == 0:
        return "(no data)"
    max_val = max(hourly)
    lines = []
    for h in range(24):
        bar_len = int(hourly[h] / max_val * 30) if max_val > 0 else 0
        bar = "█" * bar_len
        lines.append(f"{h:02d}:00 {bar} {hourly[h]}")
    return "\n".join(lines)


def render_kill_log(events: list) -> Table:
    table = Table(title="Recent Kills", show_header=True, border_style="green")
    table.add_column("#", justify="right", width=3)
    table.add_column("Time")
    table.add_column("Type")
    table.add_column("Peak", justify="right")
    table.add_column("Duration", justify="right")
    table.add_column("Energy", justify="right")
    table.add_column("Temp", justify="right")
    table.add_column("Humi", justify="right")

    for i, e in enumerate(events):
        table.add_row(
            str(i + 1),
            e["time"],
            f"{e['emoji']} {e['class']}",
            str(e["peak_adc"]),
            f"{e['duration_ms']}ms",
            str(e["energy"]),
            f"{e['temp']}°C",
            f"{e['humidity']}%",
        )
    return table


class MosKillClient:
    def __init__(self, address: str):
        self.address = address
        self.client = BleakClient(address)
        self.kill_count = 0

    async def connect(self):
        console.print(f"[cyan]Connecting to {self.address}...[/]")
        await self.client.connect()
        console.print(f"[green]Connected![/] MTU={self.client.mtu_size}")

    async def disconnect(self):
        await self.client.disconnect()
        console.print("[yellow]Disconnected.[/]")

    async def sync_time(self):
        epoch = int(time.time())
        await self.client.write_gatt_char(CHAR_TIME_SYNC, struct.pack("<I", epoch))
        console.print(f"[green]Time synced:[/] {datetime.fromtimestamp(epoch)}")

    async def read_session(self) -> dict:
        data = await self.client.read_gatt_char(CHAR_SESSION)
        return parse_session_stats(data)

    async def read_lifetime(self) -> dict:
        data = await self.client.read_gatt_char(CHAR_LIFETIME)
        return parse_lifetime_stats(data)

    async def read_kill_log(self, page: int = 0) -> list:
        data = await self.client.read_gatt_char(CHAR_KILL_LOG)
        return parse_kill_log_page(data)

    async def read_environment(self) -> dict:
        data = await self.client.read_gatt_char(CHAR_ENV)
        if len(data) >= 4:
            temp, humi = struct.unpack_from("<hH", data)
            return {"temperature": temp / 10.0, "humidity": humi / 10.0}
        return {"error": "short data"}

    async def read_battery(self) -> int:
        try:
            data = await self.client.read_gatt_char(CHAR_BATTERY)
            return data[0]
        except Exception:
            return -1

    async def write_config(self, sensitivity=1, led_bright=128, buzzer_vol=2,
                           buzzer_kill=True, led_kill=True, streak=True):
        payload = struct.pack("<BBBBBBB", sensitivity, led_bright, buzzer_vol,
                              int(buzzer_kill), int(led_kill), int(streak), 0)
        await self.client.write_gatt_char(CHAR_CONFIG, payload)
        console.print(f"[green]Config written:[/] sens={sensitivity} led={led_bright} buzz={buzzer_vol}")

    async def subscribe_kills(self, callback):
        def _handler(sender: BleakGATTCharacteristic, data: bytearray):
            count = struct.unpack_from("<I", data)[0]
            callback(count)
        await self.client.start_notify(CHAR_KILL_COUNT, _handler)

    async def subscribe_env(self, callback):
        def _handler(sender: BleakGATTCharacteristic, data: bytearray):
            if len(data) >= 4:
                temp, humi = struct.unpack_from("<hH", data)
                callback(temp / 10.0, humi / 10.0)
        await self.client.start_notify(CHAR_ENV, _handler)

    async def ota_upload(self, firmware_path: str):
        fw_data = Path(firmware_path).read_bytes()
        fw_size = len(fw_data)
        fw_crc = zlib.crc32(fw_data) & 0xFFFFFFFF

        console.print(f"[cyan]OTA: uploading {fw_size} bytes, CRC=0x{fw_crc:08X}[/]")

        # start
        start_cmd = struct.pack("<BI", 0x01, fw_size)
        await self.client.write_gatt_char(CHAR_OTA_CTRL, start_cmd)
        await asyncio.sleep(0.5)

        # write chunks
        chunk_size = 236  # MTU-safe (247 - 5 header - 6 ATT overhead)
        offset = 0
        while offset < fw_size:
            end = min(offset + chunk_size, fw_size)
            chunk = fw_data[offset:end]
            write_cmd = struct.pack("<BI", 0x02, offset) + chunk
            await self.client.write_gatt_char(CHAR_OTA_CTRL, write_cmd)
            offset = end
            pct = offset * 100 // fw_size
            if pct % 10 == 0:
                console.print(f"  [cyan]OTA progress: {pct}% ({offset}/{fw_size})[/]")
            await asyncio.sleep(0.02)

        # verify
        verify_cmd = struct.pack("<BI", 0x03, fw_crc)
        await self.client.write_gatt_char(CHAR_OTA_CTRL, verify_cmd)
        console.print("[green]OTA: verify sent, device will reboot.[/]")


async def cmd_scan(args):
    await scan_devices(timeout=args.timeout)


async def cmd_stats(args):
    mk = MosKillClient(args.address)
    await mk.connect()
    try:
        await mk.sync_time()
        session = await mk.read_session()
        lifetime = await mk.read_lifetime()
        env = await mk.read_environment()
        battery = await mk.read_battery()

        console.print(render_stats_table(session, lifetime))

        if "hourly_histogram" in lifetime:
            console.print(Panel(render_histogram(lifetime["hourly_histogram"]),
                                title="Hourly Kill Distribution"))

        console.print(f"\n[cyan]Environment:[/] {env.get('temperature', '?')}°C / {env.get('humidity', '?')}%RH")
        console.print(f"[cyan]Battery:[/] {battery}%")
    finally:
        await mk.disconnect()


async def cmd_log(args):
    mk = MosKillClient(args.address)
    await mk.connect()
    try:
        events = await mk.read_kill_log(page=args.page)
        if events:
            console.print(render_kill_log(events))
        else:
            console.print("[yellow]No kill events recorded.[/]")
    finally:
        await mk.disconnect()


async def cmd_monitor(args):
    mk = MosKillClient(args.address)
    await mk.connect()

    kill_count = [0]
    env_data = [{"temp": "--", "humi": "--"}]

    def on_kill(count):
        kill_count[0] = count
        console.print(f"  [bold green]🦟 KILL #{count}![/]")

    def on_env(temp, humi):
        env_data[0] = {"temp": f"{temp:.1f}", "humi": f"{humi:.1f}"}

    try:
        await mk.sync_time()
        await mk.subscribe_kills(on_kill)
        await mk.subscribe_env(on_env)

        console.print("[bold cyan]Monitoring... (Ctrl+C to stop)[/]")
        console.print(f"  Env: {env_data[0]['temp']}°C / {env_data[0]['humi']}%RH")

        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        console.print("\n[yellow]Monitoring stopped.[/]")
    finally:
        await mk.disconnect()


async def cmd_config(args):
    mk = MosKillClient(args.address)
    await mk.connect()
    try:
        await mk.write_config(
            sensitivity=args.sensitivity,
            led_bright=args.led,
            buzzer_vol=args.buzzer,
        )
    finally:
        await mk.disconnect()


async def cmd_ota(args):
    mk = MosKillClient(args.address)
    await mk.connect()
    try:
        await mk.ota_upload(args.firmware)
    finally:
        try:
            await mk.disconnect()
        except Exception:
            pass


def main():
    parser = argparse.ArgumentParser(description="MosKill BLE Debug Tool")
    sub = parser.add_subparsers(dest="command", required=True)

    p_scan = sub.add_parser("scan", help="Scan for MosKill devices")
    p_scan.add_argument("-t", "--timeout", type=float, default=5.0)

    p_stats = sub.add_parser("stats", help="Read kill statistics")
    p_stats.add_argument("address", help="BLE device address")

    p_log = sub.add_parser("log", help="Read kill event log")
    p_log.add_argument("address", help="BLE device address")
    p_log.add_argument("-p", "--page", type=int, default=0, help="Page number (0=newest)")

    p_mon = sub.add_parser("monitor", help="Real-time kill monitoring")
    p_mon.add_argument("address", help="BLE device address")

    p_cfg = sub.add_parser("config", help="Write device configuration")
    p_cfg.add_argument("address", help="BLE device address")
    p_cfg.add_argument("-s", "--sensitivity", type=int, default=1, choices=[0, 1, 2])
    p_cfg.add_argument("-l", "--led", type=int, default=128)
    p_cfg.add_argument("-b", "--buzzer", type=int, default=2, choices=[0, 1, 2, 3])

    p_ota = sub.add_parser("ota", help="Upload firmware via BLE OTA")
    p_ota.add_argument("address", help="BLE device address")
    p_ota.add_argument("firmware", help="Path to firmware .bin file")

    args = parser.parse_args()

    dispatch = {
        "scan": cmd_scan,
        "stats": cmd_stats,
        "log": cmd_log,
        "monitor": cmd_monitor,
        "config": cmd_config,
        "ota": cmd_ota,
    }

    asyncio.run(dispatch[args.command](args))


if __name__ == "__main__":
    main()
