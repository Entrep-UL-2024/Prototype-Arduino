# data_plotting.py

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

MAX_DATA_POINTS = 100  # Maximum number of data points to display
INTERVAL = 10  # Time (ms) between polling/plotting

def plot_data(i, pipe_conn, ir_data, red_data, current_data):
    if pipe_conn.poll():  # Check if there's data to receive
        data = pipe_conn.recv()
        # Truncate data lists if they exceed the maximum size
        if len(ir_data) > MAX_DATA_POINTS:
            ir_data.pop(0)
        if len(red_data) > MAX_DATA_POINTS:
            red_data.pop(0)
        if len(current_data) > MAX_DATA_POINTS:
            current_data.pop(0)
        if data[0] is not None : ir_data.append(data[0])
        if data[1] is not None : red_data.append(data[1])
        if data[2] is not None : current_data.append(data[2])
        print(f"IR: {ir_data}")
        print(f"Red: {red_data}")
        print(f"Current: {current_data}")

    
    plt.clf()  # Clear the entire figure
    

    plt.subplot(311)  # 3 rows, 1 column, subplot 1
    plt.plot(ir_data, label='IR')
    plt.legend(loc='upper left')
    plt.ylabel('IR Value')
    plt.title('IR Plot')
    if ir_data:
        plt.annotate(f'{ir_data[-1]}', xy=(len(ir_data)-1, ir_data[-1]), xytext=(3,3), textcoords="offset points")

    plt.subplot(312)  # 3 rows, 1 column, subplot 2
    plt.plot(red_data, label='Red')
    plt.legend(loc='upper left')
    plt.ylabel('Red Value')
    plt.title('Red Plot')
    if red_data:
        plt.annotate(f'{red_data[-1]}', xy=(len(red_data)-1, red_data[-1]), xytext=(3,3), textcoords="offset points")

    plt.subplot(313)  # 3 rows, 1 column, subplot 3
    plt.plot(current_data, label='Current')
    plt.legend(loc='upper left')
    plt.xlabel('Time (ms)')
    plt.ylabel('Current Value')
    plt.title('Current Plot')
    if current_data:
        plt.annotate(f'{current_data[-1]}', xy=(len(current_data)-1, current_data[-1]), xytext=(3,3), textcoords="offset points")

    
    plt.tight_layout()  # Ensure tight layout so it doesn't overlap

def data_plotting_process(pipe_conn):
    ir_data, red_data, current_data = [], [], []
    
    plt.figure(figsize=(8, 6)) 
    ani = FuncAnimation(plt.gcf(), plot_data, fargs=(pipe_conn, ir_data, red_data, current_data), interval=INTERVAL, cache_frame_data=False)
    plt.show()
