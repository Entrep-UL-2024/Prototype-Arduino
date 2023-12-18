import asyncio
from bleak import BleakScanner

async def scan_ble():
    devices = await BleakScanner.discover()
    for device in devices:
        print(f"Device {device.name}: {device.address}")

# On Windows and macOS, Python's asyncio event loop does not support running
# in the console's main thread. Therefore, we need to use asyncio.run() to run
# our scan_ble function.
if __name__ == "__main__":
    asyncio.run(scan_ble())
