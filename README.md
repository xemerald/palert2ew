# Module palert2ew @ Earthworm

The P-alert sensor messages reciever module based on Earthworm that developed by USGS.

## Requirement

1. It needs the [Earthworm](http://love.isti.com/trac/ew/wiki/Earthworm) environment so you need to finish the installation before compilation!
2. And you might need the MySQL optional function if so please install the lib-mysqlclient.

## installation

The version of Earthworm is under 7.9:

```
$ make ver_709
```
or
```
$ make
```

The version of Earthworm is above 7.10:

```
$ make ver_710
```

Once you need the MySQL optional function please type:

```
$ make ver_709_sql
```
or

```
$ make ver_710_sql
```

After compilation, you can find the binary file under the bin directory of Earthworm. But there still are some step need to do:

Add the lines below to the earthworm.d file:

```
Module   MOD_PALERT2EW      XXX   # XXX can be any number that is unused by other module
```
and
```
Message  TYPE_PALERTRAW     XXX   # XXX can be any number that is unused by other message type
```

Then you are able to execute this module under startstop module!

## Configuration

In fact, inside the palert2ew.d file already providing a lot of detailed information. Therefore, if you are urgent, just skip the content below directly read the configuration file.

### Basic Earthworm setup:

I recommend users do not change the parameters inside this part. However, there is one optional parameter, OutRawRing. You can define the ring for output P-alert raw packet or just comment it and close this function.

### Palert server setup:

- ServerSwitch: You can switch program mode by this parameter, that 0(Client mode) means connect to the Palert server; 1(Server mode) as the server of Palert.
- ServerIP: The server IP address of Palert server under mode 0.
- ServerPort: The server port of Palert server under mode 0.

By the way, you can ignore the parameters, ServerIP & ServerPort when switching to the mode 1.

### MySQL server information:

The alternative way for list P-alerts that will receive by this program. If you setup these parameters, especially SQLHost, the program will fetch list from MySQL server or you can just comment all of them, then it will turn off this function. And the schema of station table should include at least four columns, serial, station, network & location. Only the type of serial is number, the others are character.

The other thing, even when you using MySQL server to fetch station information, the channel table is optional. Once you comment the option, the channel information will be filled by default value(HLZ, HLN & HLE).

### Local station list:

Where to list P-alerts that will receive by this program. By the way, the priority of local list is higher than the one from remote data. And the channel codes are optional, if you don't list any of them the value will be filled by the default value.

- Normal example:

```
Palert    1993      TEST      TW         --         3         HLZ          HLN        HLE
```

- Example withour any channel code:

```
Palert    1993      TEST      TW         --         0     
```

- Optional example with maximum channel number:

```
Palert    1993      TEST      TW         --         5         HLZ          HLN        HLE        HHZ       LHZ
```
