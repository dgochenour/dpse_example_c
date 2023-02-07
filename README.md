# Working With DPSE Discovery  </br>RTI Connext DDS Micro 2.4.14, C API

## General Overview

The purpose of this example is to demonstrate how static endpoint discovery (DPSE) can be used between several RTI Connext DDS Micro applications.

> Note that actual safety-certified libraries generated from the RTI Connext Micro Cert product **only** implement the static DPSE discovery plugin, so DPDE is not an option. 

This example contains a publishing application and three subscribing applications based on a simple IDL-defined data type. Each subscribing app demonstrates a different method available to access received user data; which method is most appropriate for a user application will depend on the nature of that application.

* `example_subscriber_1.c` uses a DataReader listener to be notified of events
* `example_subscriber_2.c` uses a WaitSet
* `example_subscriber_3.c` simply polls for data

Additionally, information is presented to assist in the configuration of Admin Console when it must be used with DPSE-based Micro/Cert applications. 

* In `example_publisher.c`, the `ADMINCONSOLE` symbol can be defined (by default, it is) to configure "Admin Console-friendly" resource limits and assertions.
* Specifically, guidance and files related to the configuration of Admin Console can be found in this repo's [admin_console_config directory](./admin_console_config/).

## Source Overview

A simple "example" type, containing a message string, is defined in 
example.idl.

For the type to be useable by Connext Micro, type-support files must be 
generated that implement a type-plugin interface.  rtiddsgen can be invoked 
manually, with an example command like this:

### Windows:

    > %RTIMEHOME%\rtiddsgen\scripts\rtiddsgen -micro -language C -create typefiles example.idl

### Linux and MacOS:

    $ $RTIMEHOME/rtiddsgen/scripts/rtiddsgen -micro -language C -create typefiles ./example.idl

Note that `gcc` 11.3.0 (default under Ubuntu 22.04) breaks the pre-processor handling in the version of `rtiddsgen` delivered with Connext Micro 2.4.14. This version of Micro was released first, so it would have been impossible to anticipate the change. The workaround is simple-- if you are using `gcc` 11.3.0 simply add the `-ppOption -P` options to the arguments as shown below:


    $ $RTIMEHOME/rtiddsgen/scripts/rtiddsgen -micro -language C -ppOption -P -create typefiles ./example.idl

The generated source files are `example.c`, `exampleSupport.c`, and `examplePlugin.c`. Associated header files are also generated.
 
The DataWriter and DataReaders for this type are created and used in `example_publisher.c` and `example_subscriber_[1|2|3].c`, respectively. Each application has its own DomainParticipant since the intent is that the executables may run independently of each other.

## Generated Files Overview

### `example_publisher.c`
This file contains the logic for creating a Publisher and a DataWriter, and 
sending data.  

### `example_subscriber_1.c`
This file contains the logic for creating a Subscriber and a DataReader, a 
DataReaderListener, and listening for data.

### `example_subscriber_2.c`
This file contains the logic for creating a Subscriber and a DataReader, a 
WaitSet, and how to use the WaitSet to be notified of events.

### `example_subscriber_3.c`
This file contains the logic for creating a Subscriber and a DataReader, and  
polling is used (calling take() in a loop) to access data.

### `examplePlugin.c`
This file creates the plugin for the example data type.  This file contains 
the code for serializing and deserializing the example type, creating, 
copying, printing and deleting the example type, determining the size of the 
serialized type, and handling hashing a key, and creating the plug-in.

### `exampleSupport.c`
This file defines the example type and its typed DataWriter, DataReader, and 
Sequence.

### `example.c`
This file contains the APIs for managing the example type. 

## Environment-Specific Values

Note that the file `nic_config.h` contains information about NIC naming and the initial peer-- this may need to be changed to match your system:

    // user-configurable values
    int domain_id = 100;
    char *peer = "127.0.0.1";

    char *loopback_name = "lo";         // Ubuntu 20.04
    char *eth_nic_name = "wlp0s20f3";   // Ubuntu 20.04    

    // char *loopback_name = "Loopback Pseudo-Interface 1";    // Windows 10
    // char *eth_nic_name = "Wireless LAN adapter Wi-Fi";      // Windows 10

    // char *loopback_name = "lo0";        // MacOS 12.6.x
    // char *eth_nic_name = "en0";         // MacOS 12.6.x 

By default, the initial peer is set to the loopback address (allowing the examples to discover each other on the same machine only) and DDS Domain 100 is 
used. The names of the network interfaces should match the actual host where the example is running (defaults are provided for typical Ubuntu, Windows, and MacOS environments.)

## Compiling w/ cmake

The following assumptions are made:

* The environment variable `RTIMEHOME` is set to the Connext Micro installation directory 
* Micro libraries exist in your installation for the architecture in question
    * For example `x64Win64VS2017`, `x64Linux4gcc7.3.0`, or `x64Darwin17clang9.0` 
* If you are unsure if this is the case, please consult [the product documentation](https://community.rti.com/static/documentation/connext-micro/2.4.14/doc/html/usersmanual/index.html).


### Windows: 

    > cd your\project\directory 
    > %RTIMEHOME%\resource\scripts\rtime-make --config Release --build --name x64Win64VS2017 --target Windows --source-dir . -G "Visual Studio 15 2017 Win64" --delete

### Linux: 

    $ cd your/project/directory 
    $ $RTIMEHOME/resource/scripts/rtime-make --config Release --build --name x64Linux4gcc7.3.0 --target Linux --source-dir . -G "Unix Makefiles" --delete

After the build completes, the executables can be found in the `objs/<architecture>` directory.

## Running example_publisher and example_subscriber

Using a Windows10 system as the example, run the first subscriber in one terminal with the command:

    > objs\x64Win64VS2017\Release\example_subscriber_1.exe 
    
Run the second subscriber in another terminal with the command:

    > objs\x64Win64VS2017\Release\example_subscriber_2.exe 

Run the third subscriber in third terminal with the command:

    > objs\x64Win64VS2017\Release\example_subscriber_3.exe 

And run the publisher in a forth terminal with the command:

    > objs\x64Win64VS2017\Release\example_publisher.exe 

Linux follows a similar pattern.
