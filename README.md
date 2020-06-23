[![Build Status](https://travis-ci.org/solana-labs/ledger-app-solana.svg?branch=master)](https://travis-ci.org/solana-labs/ledger-app-solana)

# Solana app for Ledger Wallet

## Overview

This app adds support for the Solana native token to Ledger Nano S hardware wallet.

Current Features:
- Pubkey queries
- Parse, display and sign all Solana CLI generated transaction formats
- Blind sign arbitrary transactions (Enabled via settings)

## Prerequisites

Install Vagrant and VirtualBox.

Clone this git repo recursively, such that it includes the BOLOS SDK in a submodule:

```bash
$ git clone --recursive git@github.com:solana-labs/ledger-app-solana.git
cd ledger-app-solana
```

## Creating the development environment

To start the Ubuntu 18.04 VM:

```bash
$ vagrant up
```

To enter the VM:

```bash
$ vagrant ssh
```

## Alternative Setup, For those not using Vagrant

To build and install the app on your Ledger Nano S you must set up the Ledger Nano S build environments. Please follow the Getting Started instructions at [here](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html).

If you don't want to setup a global environnment, you can also setup one just for this app by sourcing `prepare-devenv.sh` with the right target (`s` or `x`).

install prerequisite and switch to a Nano X dev-env:

```bash
sudo apt install python3-venv python3-dev libudev-dev libusb-1.0-0-dev

# (x or s, depending on your device)
source prepare-devenv.sh x
```

## Building and installing

Compile:

```bash
make
```

Refresh the repo (required after Makefile edits):
```bash
make clean
```

To load the app onto the device, from the *host* machine:

```bash
make -f host.mk load
```

Remove the app from the device:

```bash
make -f host.mk delete
```


## Example of Ledger wallet functionality

```bash
cd tests
cargo run
```

## Documentation

This follows the specification available in the [`api.md`](doc/api.md).
In this project we'll create a Linux virtual machine capable of cross-compiling the
Ledger Wallet boilerplate application and then loading it onto Ledger Nano S.
