#!/usr/bin/env python

from ledgerblue.comm import getDongle
import argparse
from binascii import unhexlify
import base58

parser = argparse.ArgumentParser()
parser.add_argument('--account_number', help="BIP32 account to retrieve. e.g. \"12345\".")
args = parser.parse_args()

if args.account_number == None:
	args.account_number = "12345"

derivation_path = [44, 501, int(args.account_number)]
derivation_path_hex = '{:04x}'.format(len(derivation_path)) + "".join('{:02x}'.format(x | 0x80000000) for x in derivation_path)

# Create APDU message.
# CLA 0xE0
# INS 0x02  GET_PUBKEY
# P1 0x00   NO USER CONFIRMATION REQUIRED (0x01 otherwise)
# P2 0x00   UNUSED
payload_hex = derivation_path_hex
adpu_hex = "E0020000" + '{:02x}'.format(len(payload_hex) / 2) + payload_hex
adpu_bytes = bytearray.fromhex(adpu_hex)

print("~~ Ledger Solana ~~")
print("Request Pubkey")

dongle = getDongle(True)
result = dongle.exchange(adpu_bytes)[0:32]

print("Pubkey received: " + base58.b58encode(bytes(result)))
