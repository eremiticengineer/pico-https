# Pico HTTPS POST Test

This is a simple project to connect to wifi and POST a JSON file to a server inside a freertos task.

## Cloning the project

Clone the project with FreeRTOS submodules to get the pico functionality:

```
git clone --recurse-submodules https://github.com/eremiticengineer/pico-post-https-test
```

If you cloned without recursing submodules:

```
git submodule update --init --recursive
```

## Configuring the server and wifi information

Copy the example secrets file and add the correct information:

```
cp src/secrets.example.hpp src/secrets.hpp
```

## Getting the server root CA

The is how to get a Let's Encrypt root CA cert. If the server root CA is from a different provider the steps below may help.

```
openssl s_client -showcerts -connect server.com:443 -servername server.com </dev/null 2>/dev/null > certs.txt
```

certs.txt looks like:

```
Certificate chain
 0 s:/CN=*.server.com
   i:/C=US/O=Let's Encrypt/CN=YR2
...
 1 s:/C=US/O=Let's Encrypt/CN=YR2
   i:/C=US/O=ISRG/CN=Root YR
...
 2 s:/C=US/O=ISRG/CN=Root YR
   i:/C=US/O=Internet Security Research Group/CN=ISRG Root X1
...
Server certificate
subject=/CN=*.server.com
issuer=/C=US/O=Let's Encrypt/CN=YR2
```

from the above it's clear the server's certificate is ultimately signed by *ISRG Root X1*.

Go to [Let's Encrypt Chains of Trust](https://letsencrypt.org/certificates) and look for the link:

```
ISRG Root X1 -> Certificate details (self-signed) -> pem

isrgrootx1.pem
```

and copy the contents of *isrgrootx1.pem* to *WebServerCertificate.hpp*.

## pioasm

pioasm is invoked during the build and it takes a while to build each time:

```
[  3%] Performing build step for 'pioasmBuild'
```

so build and install it:

```
cd ${PICO_SDK_PATH}/tools/pioasm

cmake -S . -B build \
    -DPIOASM_FLAT_INSTALL=1 \
    -DPIOASM_VERSION_STRING=2.2.0

cmake --build build

sudo cmake --install build
```

which will install it in:

```
/usr/local/pioasm/pioasm
```

## FreeRTOS-Kernal setup for new projects

When creating a FreeRTOS project from scratch, clone the main branch into the project. The main branch at the moment has the necessary pico functionality:

```
git init
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git lib/FreeRTOS-Kernel
git submodule update --init --recursive
git add .gitmodules lib/FreeRTOS-Kernel
```

## FreeRTOSConfig.h

This file customises FreeRTOS for your project. The file:

```
include/FreeRTOSConfig.h
```

is this one from the pico-examples:

```
pico-examples/freertos/FreeRTOSConfig_examples_common.h
```

## References

[Task priorites](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/03-Task-priorities)
[uxTaskGetStackHighWaterMark](https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/04-uxTaskGetStackHighWaterMark)
