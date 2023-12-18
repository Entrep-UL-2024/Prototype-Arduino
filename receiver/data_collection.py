# data_collection.py

import asyncio
from bleak import BleakClient, BleakScanner, BleakGATTCharacteristic
import time

# BLE address and UUIDs
target_device_name = "ParticleSensor"
ir_uuid = "19B10011-E8F2-537E-4F6C-D104768A1214"
red_uuid = "19B10012-E8F2-537E-4F6C-D104768A1214"
current_uuid = "19B10013-E8F2-537E-4F6C-D104768A1214"

async def get_device_address(target_name):
    devices = await BleakScanner.discover()
    for device in devices:
        if device.name == target_name:
            print(f"Found device with name {target_name} and address {device.address}")
            return device.address
    return None

async def connect_and_listen_to_arduino(pipe_conn):
    arduino_ble_address = await get_device_address(target_device_name)
    if arduino_ble_address is None:
        print(f"No device with name {target_device_name} found.")
        return
    
    client = BleakClient(arduino_ble_address)
    try:
        print(f"Attempting to connect to Arduino {arduino_ble_address}...")
        await asyncio.wait_for(client.connect(), timeout=10)
        print("Connected to Arduino")
        
        def callback(sender: BleakGATTCharacteristic, data: bytearray):
            ir_value = None
            red_value = None
            current_value = None
            if sender.uuid.lower() == ir_uuid.lower():
                ir_value = int.from_bytes(data, byteorder='little')
            elif sender.uuid.lower() == red_uuid.lower():
                red_value = int.from_bytes(data, byteorder='little')
            elif sender.uuid.lower() == current_uuid.lower():
                current_value = int.from_bytes(data, byteorder='little')

            # Check if all values are available before sending them through the pipe
            if ir_value is not None or red_value is not None or current_value is not None:
                print(f"Sending IR: {ir_value}, Red: {red_value}, Current: {current_value}")
                pipe_conn.send((ir_value, red_value, current_value))

        await client.start_notify(ir_uuid, callback)
        await client.start_notify(red_uuid, callback)
        await client.start_notify(current_uuid, callback)

        # Collect data for a certain period
        await asyncio.sleep(60)  # 60 seconds
    except asyncio.TimeoutError:
        print("Connection attempt timed out.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        await client.disconnect()
        print("Client disconnected.")

def data_collection_process(pipe_conn):
    asyncio.run(connect_and_listen_to_arduino(pipe_conn))
