## Prerequisite

Make sure the assembler is assembled before doing the steps below: 


## Assembling the unit tests

Run the following script:

    ./assemble_all.sh

This will create a directory `assembled_tests`, and populate it with the assembled unit tests.

## Run the individual test

To run single test from the assembled_tests directory, use the following command:

    make <testbench_name>

Where `testbench_name` is the name of the testbench, you can find it in the assembled_tests directory. For example:
    
    make array_adder

After running the command, two directories will be created:
1) A `Logs` directory containing the test log for the testbench.
2) A `Wave` directory containing the waveform for the testbench.


## Run all the tests

To run all tests together, use the following command:
    
    make dump_all

This will create a Logs directory containing a single log file with results of all the tests and a Wave directory containing waveform files for all the tests.


## Comparison Check

To compare the output correctness of a single test, use the following command:

    diff -w Logs/<testbench_name>.log gold_output/<testbench_name>.log

For example:

    diff -w Logs/array_adder.log gold_output/array_adder.log

To compare the output correctness of all the testcases, use the following command:

    diff -w Logs/PA4_output.log gold_output/pa4_out_gold.log
.
