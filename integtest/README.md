# 10-Aug-2021, KAB: notes on some initial integrationtests...

Here is a command for fetching a file that has WIB data in it (to be used in generating emulated data):

* `curl -o frames.bin -O https://cernbox.cern.ch/index.php/s/7qNnuxD8igDOVJT/download`

Here is a sample command for invoking a test:

* `pytest -s four_process_quick_test.py --frame-file $PWD/frames.bin`

For reference, here are the ideas behind the existing tests:
* command_order_test.py - verify that only certain sequences of commands are allowed
* four_process_quick_test.py - verify that data gets written in a short run
* four_process_disabled_output_test.py - verify that the --disable-data-storage option works
* readout_type_scan.py - verify that we can write different types of data (WIB2, PDS, TPG, etc.)
* six_process_multi_file_test.py - test that the file size maximum works
* six_process_stop_start_test.py - verify that we don't get empty fragments at end run