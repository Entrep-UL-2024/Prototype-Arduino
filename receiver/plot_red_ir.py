import asyncio
from bleak import BleakClient, BleakScanner
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time

# Replace with your Arduino's BLE address
arduino_ble_name = "ParticleSensor"


# UUIDs for the BLE characteristics
ir_uuid = "19B10011-E8F2-537E-4F6C-D104768A1214"
red_uuid = "19B10012-E8F2-537E-4F6C-D104768A1214"
current_uuid = "19B10013-E8F2-537E-4F6C-D104768A1214"

# Data lists
ir_data = []
red_data = []
current_data = []

async def get_address(target_name):
    devices = await BleakScanner.discover()
    for device in devices:
        if device.name is not None and target_name == device.name and device.address is not None:
            print(f"Found {target_name} with address {device.address}")
            return device.address

async def connect_and_listen_to_arduino(arduino_ble_address):
    print(f"Creating BLE client. {arduino_ble_address}")
    client = BleakClient(arduino_ble_address)

    try:
        print("Attempting to connect")
        await asyncio.wait_for(client.connect(), timeout=3)  # 10-second timeout
        print("Connection attempt finished.")

        if client.is_connected:
            print("Connected to Arduino Nano 33 BLE")

            def callback(sender, data):
                print(f"Received data: {data} from {sender}")
                if sender == ir_uuid:
                    ir_data.append(int.from_bytes(data, byteorder='little'))
                elif sender == red_uuid:
                    red_data.append(int.from_bytes(data, byteorder='little'))
                elif sender == current_uuid:
                    current_data.append(int.from_bytes(data, byteorder='little'))

            # Subscribe to characteristics
            await client.start_notify(ir_uuid, callback)
            await client.start_notify(red_uuid, callback)
            await client.start_notify(current_uuid, callback)

            # Keep the program running to receive data
            await asyncio.sleep(60)  # Run for 60 seconds, or change as needed

            # Unsubscribe after completion
            await client.stop_notify(ir_uuid)
            await client.stop_notify(red_uuid)
            await client.stop_notify(current_uuid)
        else:
            print("Failed to connect.")
    except asyncio.TimeoutError:
        print("Connection attempt timed out.")
    except Exception as e:
        print(f"An error occurred during BLE operation: {e}")
    finally:
        if client.is_connected:
            await client.disconnect()
        print("Client disconnected.")

def plot_data(i):
    plt.cla()  # Clear the plot
    plt.plot(ir_data, label='IR')
    plt.plot(red_data, label='Red')
    plt.plot(current_data, label='Current')
    plt.legend(loc='upper left')
    plt.xlabel('Time')
    plt.ylabel('Value')
    plt.title('Real-time Plotting')

async def run_ble_client(address):
    await connect_and_listen_to_arduino(address)

async def run_plotting():
    plt.figure()
    ani = FuncAnimation(plt.gcf(), plot_data, interval=1000, cache_frame_data=False)
    plt.tight_layout()
    plt.show()

async def main():
    address = await get_address(arduino_ble_name)
    if address:
        print(f"Arduino address found: {address}")
        # Run BLE client and plotting in parallel
        await asyncio.gather(run_ble_client(address), run_plotting())
    else:
        print("Arduino device not found. Please check if the device is advertising.")


if __name__ == "__main__":
    asyncio.run(main())

    # loop = asyncio.get_event_loop()
    # arduino_ble_address = loop.run_until_complete(get_address(arduino_ble_name))
    # if arduino_ble_address:
    #     print(f"Arduino address found: {arduino_ble_address}")
    #     loop.run_until_complete(connect_and_listen_to_arduino(arduino_ble_address))
    #     # plt.figure()
    #     # ani = FuncAnimation(plt.gcf(), plot_data, interval=1000, cache_frame_data=False)
    #     # plt.tight_layout()
    #     # plt.show()
    # else:
    #     print("Arduino device not found. Please check if the device is advertising.")
