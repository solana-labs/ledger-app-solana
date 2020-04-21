Solana application : Common Technical Specifications
=======================================================

## About

This application describes the APDU messages interface to communicate with the Solana application.

The application covers the following functionalities :

   - Retrieve an address given an account number

The application interface can be accessed over HID or BLE

## General purpose APDUs

### GET APP CONFIGURATION

#### Description

_This command returns specific application configuration_

##### Command

| *CLA*         | *INS*         | *P1*  | *P2*          | *Lc*          | *Le*   |
| ------------- |:-------------:| -----:| ------------- |:-------------:| -----: |
| E0            | 04            | 00    | 00            | 00            | 04     |

##### Input data

_None_

##### Output data


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Dummy setting n°1 value                                                           | 01
| Dummy setting n°2 value                                                           | 01
| Application major version                                                         | 01
| Application minor version                                                         | 01
| Application patch version                                                         | 01


### GET PUBKEY

#### Description

_This command returns a Solana pubkey for the given BIP 32 path_

##### Command

| *CLA*         | *INS*         | *P1*  | *P2*          | *Lc*          | *Le*   |
| ------------- |:-------------:| -----:| ------------- |:-------------:| -----: |
| E0            | 05            | 00    | 00            | variable          | variable     |



##### Input data


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Number of BIP 32 derivations to perform (3 or 4)                                  | 1
| First derivation index (big endian)                                               | 4
| ...                                                                               | 4
| Last derivation index (big endian)                                                | 4

##### Output data


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Pubkey                                                                            | 32


### SIGN SOLANA TRANSACTION

#### Description

_This command signs a Solana Transaction after having the user validate the transaction-specific parameters:_

##### Command


| *CLA* | *INS*  | *P1*               | *P2*       | *Lc*     | *Le*
| ------------- |:-------------:| -----:| ------------- |:-------------:| -----: |
|   E0  |   06   |  01                |   00       | variable | variable


##### Input data


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Number of signers (derivation paths) (always 1)                                   | 1
| Number of BIP 32 derivations to perform (2, 3 or 4)                               | 1
| First derivation index (big endian)                                               | 4
| ...                                                                               | 4
| Last derivation index (big endian)                                                | 4
| Serialized transaction                                                            | variable

##### Output data


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Signature                                                                         | 64


## Transport protocol

### General transport description

_Ledger APDUs requests and responses are encapsulated using a flexible protocol allowing to fragment large payloads over different underlying transport mechanisms._

The common transport header is defined as follows:


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| Communication channel ID (big endian)                                             | 2
| Command tag                                                                       | 1
| Packet sequence index (big endian)                                                | 2
| Payload                                                                           | var


The Communication channel ID allows commands multiplexing over the same physical link. It is not used for the time being, and should be set to 0101 to avoid compatibility issues with implementations ignoring a leading 00 byte.

The Command tag describes the message content. Use TAG_APDU (0x05) for standard APDU payloads, or TAG_PING (0x02) for a simple link test.

The Packet sequence index describes the current sequence for fragmented payloads. The first fragment index is 0x00.

### APDU Command payload encoding

APDU Command payloads are encoded as follows :


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| APDU length (big endian)                                                          | 2
| APDU CLA                                                                          | 1
| APDU INS                                                                          | 1
| APDU P1                                                                           | 1
| APDU P2                                                                           | 1
| APDU data length                                                                  | 1
| Optional APDU data                                                                | var

APDU payload is encoded according to the APDU case


| Case Number  | *Lc* | *Le* | Case description
| -----------  | ---- | ---- | ----------------
|   1          |  0   |  0   | No data in either direction - L is set to 00
|   2          |  0   |  !0  | Input Data present, no Output Data - L is set to Lc
|   3          |  !0  |  0   | Output Data present, no Input Data - L is set to Le
|   4          |  !0  |  !0  | Both Input and Output Data are present - L is set to Lc

#### Deprecation notice

The `ADPU data length` field was formerly serialized as a 16bit unsigned big endian integer. As of version 0.2.0, this has been changed to an 8bit unsigned integer to improve compatibility with client libraries. In doing so, the following instructions have been deprecated.

- 0x01 - GET_APP_CONFIGURATION
- 0x02 - GET_PUBKEY
- 0x03 - SIGN_MESSAGE

### APDU Response payload encoding

APDU Response payloads are encoded as follows :


| *Description*                                                                     | *Length*
| ------------- |:-------------:|
| APDU response length (big endian)                                                 | 2
| APDU response data and Status Word                                                | var


### USB mapping

Messages are exchanged with the dongle over HID endpoints over interrupt transfers, with each chunk being 64 bytes long. The HID Report ID is ignored.

### BLE mapping

A similar encoding is used over BLE, without the Communication channel ID.

The application acts as a GATT server defining service UUID D973F2E0-B19E-11E2-9E96-0800200C9A66

When using this service, the client sends requests to the characteristic D973F2E2-B19E-11E2-9E96-0800200C9A66, and gets notified on the characteristic D973F2E1-B19E-11E2-9E96-0800200C9A66 after registering for it.

Requests are encoded using the standard BLE 20 bytes MTU size

## Status Words

The following standard Status Words are returned for all APDUs - some specific Status Words can be used for specific commands and are mentioned in the command description.

##### Status Words

| *SW*     | *Description*
| ------------- |:-------------:|
|   6700   | Incorrect length
|   6982   | Security status not satisfied (Canceled by user)
|   6A80   | Invalid data
|   6B00   | Incorrect parameter P1 or P2
|   6Fxx   | Technical problem (Internal error, please report)
|   9000   | Normal ending of the command
