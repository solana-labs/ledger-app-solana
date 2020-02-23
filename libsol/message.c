#include "sol/parser.h"
#include "sol/printer.h"
#include "sol/message.h"
#include "system_instruction.h"
#include "stake_instruction.h"
#include <string.h>
#include <stdio.h>

int process_message_body(uint8_t* message_body, int message_body_length, MessageHeader* header, field_t* fields, size_t* fields_used) {
    Parser parser = {message_body, message_body_length};
    char pubkeyBuffer[BASE58_PUBKEY_LENGTH];

    // Check if these are system instructions
    SystemTransferInfo transfer_info;
    if (parse_system_transfer_instructions(&parser, header, &transfer_info) == 0) {
        strcpy(fields[0].title, "Transfer");
        print_amount(transfer_info.lamports, "SOL", fields[0].text);

        strcpy(fields[1].title, "Sender");
        encode_base58((uint8_t*) transfer_info.from, PUBKEY_SIZE, (uint8_t*) pubkeyBuffer, BASE58_PUBKEY_LENGTH);
        print_summary(pubkeyBuffer, fields[1].text, SUMMARY_LENGTH, SUMMARY_LENGTH);

        strcpy(fields[2].title, "Recipient");
        encode_base58((uint8_t*) transfer_info.to, PUBKEY_SIZE, (uint8_t*) pubkeyBuffer, BASE58_PUBKEY_LENGTH);
        print_summary(pubkeyBuffer, fields[2].text, SUMMARY_LENGTH, SUMMARY_LENGTH);

        if (memcmp(&header->pubkeys[0], transfer_info.to, PUBKEY_SIZE) == 0) {
            snprintf(fields[3].text, BASE58_PUBKEY_LENGTH, "recipient");
        }

        if (memcmp(&header->pubkeys[0], transfer_info.from, PUBKEY_SIZE) == 0) {
            snprintf(fields[3].text, BASE58_PUBKEY_LENGTH, "sender");
        }

        *fields_used = 4;
        return 0;
    }

    // Reset the parser
    parser.buffer = message_body;
    parser.buffer_length = message_body_length;

    // Check if these are staking instructions
    DelegateStakeInfo delegate_info;
    if (parse_delegate_stake_instructions(&parser, header, &delegate_info) == 0) {
        strcpy(fields[0].title, "Delegate from");
        encode_base58((uint8_t*) delegate_info.stake_pubkey, PUBKEY_SIZE, (uint8_t*) pubkeyBuffer, BASE58_PUBKEY_LENGTH);
        print_summary(pubkeyBuffer, fields[0].text, SUMMARY_LENGTH, SUMMARY_LENGTH);

        strcpy(fields[1].title, "Authorized by");
        encode_base58((uint8_t*) delegate_info.authorized_pubkey, PUBKEY_SIZE, (uint8_t*) pubkeyBuffer, BASE58_PUBKEY_LENGTH);
        print_summary(pubkeyBuffer, fields[1].text, SUMMARY_LENGTH, SUMMARY_LENGTH);

        strcpy(fields[2].title, "Vote account");
        encode_base58((uint8_t*) delegate_info.vote_pubkey, PUBKEY_SIZE, (uint8_t*) pubkeyBuffer, BASE58_PUBKEY_LENGTH);
        print_summary(pubkeyBuffer, fields[2].text, SUMMARY_LENGTH, SUMMARY_LENGTH);

        if (memcmp(&header->pubkeys[0], delegate_info.authorized_pubkey, PUBKEY_SIZE) == 0) {
            snprintf(fields[3].text, BASE58_PUBKEY_LENGTH, "authorizer");
        }

        *fields_used = 4;
        return 0;
    }

    *fields_used = 0;
    return 1;
}
