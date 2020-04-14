#include "common_byte_strings.h"
#include "sol/parser.h"
#include "sol/transaction_summary.h"
#include "stake_instruction.h"
#include "util.h"
#include <stdbool.h>
#include <string.h>

const Pubkey stake_program_id = {{
    PROGRAM_ID_STAKE
}};

static int parse_stake_instruction_kind(Parser* parser, enum StakeInstructionKind* kind) {
    uint32_t maybe_kind;
    BAIL_IF(parse_u32(parser, &maybe_kind));
    switch (maybe_kind) {
        case StakeInitialize:
        case StakeAuthorize:
        case StakeDelegate:
        case StakeSplit:
        case StakeWithdraw:
        case StakeDeactivate:
        case StakeSetLockup:
            *kind = (enum StakeInstructionKind) maybe_kind;
            return 0;
    }
    return 1;
}

static int parse_stake_authorize(
    Parser* parser,
    enum StakeAuthorize* authorize
) {
    uint32_t maybe_authorize;
    BAIL_IF(parse_u32(parser, &maybe_authorize));
    switch (maybe_authorize) {
        case StakeAuthorizeStaker:
        case StakeAuthorizeWithdrawer:
            *authorize = (enum StakeAuthorize) maybe_authorize;
            return 0;
    }
    return 1;
}

// Returns 0 and populates StakeDelegateInfo if provided a MessageHeader and a delegate
// instruction, otherwise non-zero.
static int parse_delegate_stake_instruction(const Instruction* instruction, const MessageHeader* header, StakeDelegateInfo* info) {
    BAIL_IF(instruction->accounts_length < 6);
    size_t pubkeys_length = header->pubkeys_header.pubkeys_length;
    uint8_t accounts_index = 0;
    uint8_t pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    info->stake_pubkey = &header->pubkeys[pubkeys_index];

    pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    info->vote_pubkey = &header->pubkeys[pubkeys_index];

    pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    //const Pubkey* pubkey = &header->pubkeys[pubkeys_index];
    //BAIL_IF(memcmp(pubkey, &clock_pubkey, PUBKEY_SIZE));

    pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    //pubkey = &header->pubkeys[pubkeys_index];
    //BAIL_IF(memcmp(pubkey, &stake_history_pubkey, PUBKEY_SIZE));

    pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    //pubkey = &header-:pubkeys[pubkeys_index];
    //BAIL_IF(memcmp(pubkey, &config_pubkey, PUBKEY_SIZE));

    pubkeys_index = instruction->accounts[accounts_index++];
    BAIL_IF(pubkeys_index >= pubkeys_length);
    info->authorized_pubkey = &header->pubkeys[pubkeys_index];

    return 0;
}

static int parse_stake_initialize_instruction(Parser* parser, const Instruction* instruction, const MessageHeader* header, StakeInitializeInfo* info) {
    BAIL_IF(instruction->accounts_length < 2);
    size_t accounts_index = 0;
    uint8_t pubkeys_index = instruction->accounts[accounts_index++];
    info->account = &header->pubkeys[pubkeys_index];

    accounts_index++; // Skip rent sysvar

    BAIL_IF(parse_pubkey(parser, &info->stake_authority));
    BAIL_IF(parse_pubkey(parser, &info->withdraw_authority));

    // Lockup
    BAIL_IF(parse_i64(parser, &info->lockup.unix_timestamp));
    BAIL_IF(parse_u64(parser, &info->lockup.epoch));
    BAIL_IF(parse_pubkey(parser, &info->lockup.custodian));
    info->lockup. present = StakeLockupHasAll;

    return 0;
}

static int parse_stake_withdraw_instruction(Parser* parser, const Instruction* instruction, const MessageHeader* header, StakeWithdrawInfo* info) {
    BAIL_IF(instruction->accounts_length < 5);
    size_t accounts_index = 0;
    size_t pubkeys_index = instruction->accounts[accounts_index++];
    info->account = &header->pubkeys[pubkeys_index];

    pubkeys_index = instruction->accounts[accounts_index++];
    info->to = &header->pubkeys[pubkeys_index];

    accounts_index++; // Skip clock sysvar
    accounts_index++; // Skip stake history sysvar

    pubkeys_index = instruction->accounts[accounts_index++];
    info->authority = &header->pubkeys[pubkeys_index];

    BAIL_IF(parse_u64(parser, &info->lamports));

    return 0;
}

static int parse_stake_authorize_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    StakeAuthorizeInfo* info
) {
    BAIL_IF(instruction->accounts_length < 3);
    size_t accounts_index = 0;
    size_t pubkeys_index = instruction->accounts[accounts_index++];
    info->account = &header->pubkeys[pubkeys_index];

    accounts_index++; // Skip clock sysvar

    pubkeys_index = instruction->accounts[accounts_index++];
    info->authority = &header->pubkeys[pubkeys_index];

    BAIL_IF(parse_pubkey(parser, &info->new_authority));
    BAIL_IF(parse_stake_authorize(parser, &info->authorize));

    return 0;
}

static int parse_stake_deactivate_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    StakeDeactivateInfo* info
) {
    BAIL_IF(instruction->accounts_length < 3);
    size_t accounts_index = 0;
    size_t pubkeys_index = instruction->accounts[accounts_index++];
    info->account = &header->pubkeys[pubkeys_index];

    accounts_index++; // Skip clock sysvar

    pubkeys_index = instruction->accounts[accounts_index++];
    info->authority = &header->pubkeys[pubkeys_index];

    return 0;
}

int parse_stake_instructions(const Instruction* instruction, const MessageHeader* header, StakeInfo* info) {
    Parser parser = {instruction->data, instruction->data_length};

    BAIL_IF(parse_stake_instruction_kind(&parser, &info->kind));

    switch (info->kind) {
        case StakeDelegate:
            return parse_delegate_stake_instruction(instruction, header, &info->delegate_stake);
        case StakeInitialize:
            return parse_stake_initialize_instruction(&parser, instruction, header, &info->initialize);
        case StakeWithdraw:
            return parse_stake_withdraw_instruction(&parser, instruction, header, &info->withdraw);
        case StakeAuthorize:
            return parse_stake_authorize_instruction(
                &parser,
                instruction,
                header,
                &info->authorize
            );
        case StakeDeactivate:
            return parse_stake_deactivate_instruction(
                &parser,
                instruction,
                header,
                &info->deactivate
            );
        case StakeSplit:
        case StakeSetLockup:
            break;
    }

    return 1;
}

static int print_delegate_stake_info(const StakeDelegateInfo* info, const MessageHeader* header) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Delegate from", info->stake_pubkey);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Authorized by", info->authorized_pubkey);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Vote account", info->vote_pubkey);

    item = transaction_summary_fee_payer_item();
    if (memcmp(&header->pubkeys[0], info->authorized_pubkey, PUBKEY_SIZE) == 0) {
        transaction_summary_set_fee_payer_string("authorizer");
    }

    return 0;
}

static int print_stake_withdraw_info(const StakeWithdrawInfo* info, const MessageHeader* header) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_amount(item, "Stake withdraw", info->lamports);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "From", info->account);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "To", info->to);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Authorized by", info->authority);

    return 0;
}

static int print_stake_authorize_info(
    const StakeAuthorizeInfo* info,
    const MessageHeader* header
) {
    const char* new_authority_title = NULL;
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Set stake auth.", info->account);

    switch (info->authorize) {
        case StakeAuthorizeStaker:
            new_authority_title = "New stake auth.";
            break;
        case StakeAuthorizeWithdrawer:
            new_authority_title = "New w/d auth.";
            break;
    }

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, new_authority_title, info->new_authority);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Authorized by", info->authority);

    return 0;
}

static int print_stake_deactivate_info(
    const StakeDeactivateInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Deactivate stake", info->account);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Authorized by", info->authority);

    return 0;
}

int print_stake_info(const StakeInfo* info, const MessageHeader* header) {
    switch (info->kind) {
        case StakeDelegate:
            return print_delegate_stake_info(&info->delegate_stake, header);
        case StakeInitialize:
            return print_stake_initialize_info("Init. stake acct", &info->initialize, header);
        case StakeWithdraw:
            return print_stake_withdraw_info(&info->withdraw, header);
        case StakeAuthorize:
            return print_stake_authorize_info(&info->authorize, header);
        case StakeDeactivate:
            return print_stake_deactivate_info(&info->deactivate, header);
        case StakeSplit:
        case StakeSetLockup:
            break;
    }

    return 1;
}

int print_stake_initialize_info(
    const char* primary_title,
    const StakeInitializeInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;
    bool one_authority = pubkeys_equal(
        info->withdraw_authority,
        info->stake_authority
    );

    if (primary_title != NULL) {
        item = transaction_summary_primary_item();
        summary_item_set_pubkey(item, primary_title, info->account);
    }

    if (one_authority) {
        item = transaction_summary_general_item();
        summary_item_set_pubkey(item, "New authority", info->stake_authority);
    } else {
        item = transaction_summary_general_item();
        summary_item_set_pubkey(item, "New stake auth", info->stake_authority);

        item = transaction_summary_general_item();
        summary_item_set_pubkey(item, "New withdraw auth", info->withdraw_authority);
    }

    item = transaction_summary_general_item();
    summary_item_set_i64(item, "Lockup time", info->lockup.unix_timestamp);

    item = transaction_summary_general_item();
    summary_item_set_u64(item, "Lockup epoch", info->lockup.epoch);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Lockup custodian", info->lockup.custodian);

    return 0;
}
