# data_collection.py

import asyncio
from bleak import BleakClient, BleakScanner, BleakGATTCharacteristic
import time
import logging
import backoff

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# BLE address and UUIDs
target_device_name = "ParticleSensor"
hr_uuid = "19B10011-E8F2-537E-4F6C-D104768A1214"
spo2_uuid = "19B10012-E8F2-537E-4F6C-D104768A1214"
current_uuid = "19B10013-E8F2-537E-4F6C-D104768A1214"

device_discovered = False
client = None

async def get_device_address(target_name):
    global device_discovered
    devices = await BleakScanner.discover()
    for device in devices:
        if device.name == target_name:
            logger.info(f"Found device with name {target_name} and address {device.address}")
            device_discovered = True
            return device.address
    return None

async def get_client():
    global client, device_discovered
    if device_discovered:
        return client
    arduino_ble_address = await get_device_address(target_device_name)
    if arduino_ble_address is None:
        logger.warning(f"No device with name {target_device_name} found.")
        return None
    client = BleakClient(arduino_ble_address)
    logger.info(f"Attempting to connect to Arduino {arduino_ble_address}...")
    return client

async def setup_notifications(client, callback):
    if client is not None:
        await client.start_notify(hr_uuid, callback)
        await client.start_notify(spo2_uuid, callback)
        await client.start_notify(current_uuid, callback)

def data_callback(sender: BleakGATTCharacteristic, data: bytearray, pipe_conn):
    hr_value = None
    spo2_value = None
    current_value = None
    if sender.uuid.lower() == hr_uuid.lower():
        hr_value = int.from_bytes(data, byteorder='little')
    elif sender.uuid.lower() == spo2_uuid.lower():
        spo2_value = int.from_bytes(data, byteorder='little')
    elif sender.uuid.lower() == current_uuid.lower():
        current_value = int.from_bytes(data, byteorder='little')

    if hr_value is not None or spo2_value is not None or current_value is not None:
        logger.info(f"Sending IR: {hr_value}, Red: {spo2_value}, Current: {current_value}")
        pipe_conn.send((hr_value, spo2_value, current_value))

@backoff.on_exception(backoff.expo, Exception, max_time=300)
async def connect_and_listen_to_arduino(pipe_conn):
    global client
    try:
        client = await get_client()
        if(client is None):
            logger.info("No client got 1 ...")
            return
        await asyncio.wait_for(client.connect(), timeout=15)
        logger.info("Connected to Arduino")

        await setup_notifications(client, lambda s, d: data_callback(s, d, pipe_conn))

        while client is not None and client.is_connected:
            await asyncio.sleep(3)

    except Exception as e:
        logger.error(f"An error occurred: {e}")
    finally:
        if client is not None and client.is_connected:
            await client.disconnect()
        logger.info("Client disconnected. Will attempt to reconnect...")

async def connect_to_device(target_name):
    address = await get_device_address(target_name)
    if address is None:
        logger.warning(f"No device with name {target_name} found.")
        return False

    client = BleakClient(address)
    try:
        await client.connect()
        logger.info(f"Connected to {target_name} at address {address}")
        return True
    except Exception as e:
        logger.error(f"An error occurred while connecting: {e}")
        return False
    finally:
        if client.is_connected:
            await client.disconnect()
            logger.info("Disconnected from the device")

async def main(pipe_conn):
    global client
    try:
        while True:
            await connect_and_listen_to_arduino(pipe_conn)
            await asyncio.sleep(5)
    except KeyboardInterrupt:
        logger.info("Keyboard interrupt detected. Exiting...")
        if client is not None and client.is_connected:
            await client.disconnect()
        logger.info("Cleanup complete. Goodbye!")
        pipe_conn.close()

    # success = await connect_to_device(target_device_name)
    # if success:
    #     logger.info("Device connection was successful")
    # else:
    #     logger.info("Device connection failed")

def data_collection_process(pipe_conn):
    asyncio.run(main(pipe_conn))
