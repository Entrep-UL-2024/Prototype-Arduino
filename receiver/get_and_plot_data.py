# main.py

import multiprocessing
from data_collection import data_collection_process
from data_plotting import data_plotting_process

if __name__ == "__main__":
    parent_conn, child_conn = multiprocessing.Pipe()

    # Create data collection process
    data_collector = multiprocessing.Process(target=data_collection_process, args=(child_conn,))
    data_collector.start()

    # Create data plotting process
    data_plotter = multiprocessing.Process(target=data_plotting_process, args=(parent_conn,))
    data_plotter.start()

    data_collector.join()
    data_plotter.join()
