#include "sol/parser.h"
#include "util.h"

static int check_buffer_length(Parser* parser, size_t num) {
    return parser->buffer_length < num ? 1 : 0;
}

static void advance(Parser* parser, size_t num) {
    parser->buffer += num;
    parser->buffer_length -= num;
}

static int parse_u8(Parser* parser, uint8_t* value) {
    BAIL_IF(check_buffer_length(parser, 1));
    *value = *parser->buffer;
    advance(parser, 1);
    return 0;
}

static int parse_u16(Parser* parser, uint16_t* value) {
    uint8_t lower, upper;
    BAIL_IF(parse_u8(parser, &lower));
    BAIL_IF(parse_u8(parser, &upper));
    *value = lower + ((uint16_t)upper << 8);
    return 0;
}

int parse_u32(Parser* parser, uint32_t* value) {
    uint16_t lower, upper;
    BAIL_IF(parse_u16(parser, &lower));
    BAIL_IF(parse_u16(parser, &upper));
    *value = lower + ((uint32_t)upper << 16);
    return 0;
}

int parse_u64(Parser* parser, uint64_t* value) {
    BAIL_IF(check_buffer_length(parser, 8));
    uint32_t lower, upper;
    BAIL_IF(parse_u32(parser, &lower));
    BAIL_IF(parse_u32(parser, &upper));
    *value = lower + ((uint64_t)upper << 32);
    return 0;
}

int parse_length(Parser* parser, size_t* value) {
    uint8_t value_u8;
    BAIL_IF(parse_u8(parser, &value_u8));
    *value = value_u8 & 0x7f;

    if (value_u8 & 0x80) {
        BAIL_IF(parse_u8(parser, &value_u8));
        *value = ((value_u8 & 0x7f) << 7) | *value;
        if (value_u8 & 0x80) {
            BAIL_IF(parse_u8(parser, &value_u8));
            *value = ((value_u8 & 0x7f) << 14) | *value;
	}
    }
    return 0;
}

int parse_pubkeys_header(Parser* parser, PubkeysHeader* header) {
    BAIL_IF(parse_u8(parser, &header->num_required_signatures));
    BAIL_IF(parse_u8(parser, &header->num_readonly_signed_accounts));
    BAIL_IF(parse_u8(parser, &header->num_readonly_unsigned_accounts));
    BAIL_IF(parse_length(parser, &header->pubkeys_length));
    return 0;
}

int parse_pubkeys(Parser* parser, PubkeysHeader* header, Pubkey** pubkeys) {
    BAIL_IF(parse_pubkeys_header(parser, header));
    size_t pubkeys_size = header->pubkeys_length * PUBKEY_SIZE;
    BAIL_IF(check_buffer_length(parser, pubkeys_size));
    *pubkeys = (Pubkey*) parser->buffer;
    advance(parser, pubkeys_size);
    return 0;
}

int parse_hash(Parser* parser, Hash** hash) {
    BAIL_IF(check_buffer_length(parser, HASH_SIZE));
    *hash = (Hash*) parser->buffer;
    advance(parser, HASH_SIZE);
    return 0;
}

int parse_message_header(Parser* parser, MessageHeader* header) {
    BAIL_IF(parse_pubkeys(parser, &header->pubkeys_header, &header->pubkeys));
    BAIL_IF(parse_blockhash(parser, &header->blockhash));
    BAIL_IF(parse_length(parser, &header->instructions_length));
    return 0;
}

static int parse_data(Parser* parser, uint8_t** data, size_t* data_length) {
    BAIL_IF(parse_length(parser, data_length));
    BAIL_IF(check_buffer_length(parser, *data_length));
    *data = parser->buffer;
    advance(parser, *data_length);
    return 0;
}

int parse_instruction(Parser* parser, Instruction* instruction) {
    BAIL_IF(parse_u8(parser, &instruction->program_id_index));
    BAIL_IF(parse_data(parser, &instruction->accounts, &instruction->accounts_length));
    BAIL_IF(parse_data(parser, &instruction->data, &instruction->data_length));
    return 0;
}
