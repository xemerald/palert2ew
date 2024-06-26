# Module palert2ew @ Earthworm

The P-Alert sensor messages receiving module based on Earthworm that developed by USGS.

## Requirement

1. It needs the [Earthworm](http://www.earthwormcentral.org/) environment so you need to finish the installation before compilation!
2. And you might need the MySQL optional function if so please install the **lib-mysqlclient**.

## Build & Installation

The version of Earthworm is under **7.9** (this might be obsoleted later):

```
$ make ver_709
```

The version of Earthworm is above **7.10**:

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

1. First, add the lines below to the earthworm.d file:

```
Module   MOD_PALERT2EW      XXX   # XXX can be any number that is unused by other module
```
and
```
Message  TYPE_PALERTRAW     XXX   # XXX can be any number that is unused by other message type
```

2. Second, copy the configuration file **palert2ew.d** to the param directory of Earthworm.

3. Final, 'cause the P-Alert use the modbus protocol, the listening port of Server mode is **502**. And under UNIX-like system it need the **superuser permission** or you can execute the command below to enable the capability of binding the port below 1024:

```
$ sudo setcap cap_net_bind_service=+ep <PATH_TO_THE_BINARY_FILE>
```
or
```
$ make cap_set
```
Then you are able to execute this module under startstop module!

## Configuration

In fact, inside the palert2ew.d file already providing a lot of detailed information. Therefore, if you are really urgent, just skip the content below directly read the configuration file.

### Basic Earthworm setup

I recommend users do not change the parameters inside this part. However, there is an optional parameter, OutRawRing. You can define the ring for output P-Alert raw packet or just comment it and close this function.

### Data quality setup

The new function for those who care about the data quality & integrity. First, since 2022 the P-Alert sensors add the CRC-16 check sum into the packet include mode 1, 4 & 16. By this check sum, this program is able to ensure the integrity of the receiving packets to avoid those waveform glitches & anomalies. Second, sometimes the P-Alert sensors would lose the connection to NTP server which will also cause gaps between waveforms. Therefore, for those who care about data continuity, this program can still output the time questionable waveforms with special mark if you turn on the function.

- *CheckCRC16* : That 0 (default) means turn off the checking process of CRC-16; 1 means turn it on. Since the checking process takes some time; Therefore, if you care about the timeliness, 0 is preferable.
- *OutputTimeQuestionable* : That 0 means (default) to filter out those waveforms from NTP unsynchronized stations; 1 means to allow those waveforms with questionable timestamp.

### Output data type setup

The common data type within Earthworm is 4 bytes integer, so as the output of P-Alert mode 1 & 4 packets. However, the raw data type of P-Alert mode 16 packet is [IEEE-754 float](https://en.wikipedia.org/wiki/IEEE_754). Here, concerning the timeliness, the program default to output the data with float type. Once you want to keep the consitency of the data type, you can turn on this function to convert the float data to integer data.

- *ForceOutputIntData* : That 0 (default) means **keep the raw data type** from packets; 1 means **force to output integer data type**, especially for mode 16 packets.

### Palert server setup

- *ServerSwitch* : You can switch program mode by this parameter, that 0, **Client mode** means connect to the Palert server; 1, **Server mode** as the server of Palert.
- *ServerIP* : The server IP address of Palert server under mode 0.
- *ServerPort* : The server port of Palert server under mode 0.

By the way, **you can skip the parameters, ServerIP & ServerPort when switching to the mode 1.**

### MySQL server information

The alternative way for list P-Alerts that will receive by this program. If you setup these parameters, **especially SQLHost**, the program will fetch list from MySQL server or you can just comment all of them, then it will turn off this function. And the schema of station table should include at least four columns, serial, station, network & location. Only the type of serial is number, the others are character.

The other thing, even when you using MySQL server to fetch station information, the channel table is optional. Once you comment the option, the channel information will be filled by default value(HLZ, HLN & HLE).

### Local station list

Where to list P-Alerts that will receive by this program. By the way, the priority of local list is higher than the one from remote data. And the channel codes are optional, if you don't list any of them the value will be filled by the default value.

- Normal example:

```
Palert    1993      TEST      TW         --         3         HLZ          HLN        HLE
```

- Example without any channel code:

```
Palert    1993      TEST      TW         --         0
```

- Optional example with **maximum channel number, which is 8**:

```
Palert    1993      TEST      TW         --         8         HLZ          HLN        HLE        HHZ       HHN       HHE       LHZ       LHN
```
