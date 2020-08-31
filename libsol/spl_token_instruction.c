#include "common_byte_strings.h"
#include "instruction.h"
#include "sol/parser.h"
#include "sol/transaction_summary.h"
#include "spl_token_instruction.h"
#include "util.h"

const Pubkey spl_token_program_id = {{
    PROGRAM_ID_SPL_TOKEN
}};

static int parse_spl_token_instruction_kind(
    Parser* parser,
    SplTokenInstructionKind* kind
) {
    uint8_t maybe_kind;
    BAIL_IF(parse_u8(parser, &maybe_kind));
    switch (maybe_kind) {
        case SplTokenKind(InitializeMint):
        case SplTokenKind(InitializeAccount):
        case SplTokenKind(InitializeMultisig):
        case SplTokenKind(Transfer):
        case SplTokenKind(Approve):
        case SplTokenKind(Revoke):
        case SplTokenKind(SetAuthority):
        case SplTokenKind(MintTo):
        case SplTokenKind(Burn):
        case SplTokenKind(CloseAccount):
            *kind = (SplTokenInstructionKind) maybe_kind;
            return 0;
        case SplTokenKind(FreezeAccount):
        case SplTokenKind(ThawAccount):
        case SplTokenKind(Transfer2):
        case SplTokenKind(Approve2):
        case SplTokenKind(MintTo2):
        case SplTokenKind(Burn2):
            break;
    }
    return 1;
}

static int parse_initialize_mint_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenInitializeMintInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u8(parser, &info->decimals));
    BAIL_IF(parse_pubkey(parser, &info->mint_authority));
    enum Option freeze_authority;
    BAIL_IF(parse_option(parser, &freeze_authority));
    if (freeze_authority == OptionSome) {
        BAIL_IF(parse_pubkey(parser, &info->freeze_authority));
    } else {
        info->freeze_authority = NULL;
    }

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->mint_account));

    return 0;
}

static int parse_initialize_account_spl_token_instruction(
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenInitializeAccountInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->mint_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->owner));

    return 0;
}

static int parse_spl_token_multisigners(
    InstructionAccountsIterator* it,
    SplTokenMultisigners* signers
) {
    size_t n = instruction_accounts_iterator_remaining(it);
    BAIL_IF(n > Token_MAX_SIGNERS);
    BAIL_IF(instruction_accounts_iterator_next(it, &signers->first));
    signers->count = n;

    return 0;
}

static int parse_initialize_multisig_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenInitializeMultisigInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u8(parser, &info->body.m));
    BAIL_IF(info->body.m > Token_MAX_SIGNERS);

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->multisig_account));
    BAIL_IF(parse_spl_token_multisigners(&it, &info->signers))

    return 0;
}

static int parse_spl_token_sign(
    InstructionAccountsIterator* it,
    SplTokenSign* sign
) {
    size_t n = instruction_accounts_iterator_remaining(it);
    BAIL_IF(n == 0);

    if (n == 1) {
        sign->kind = SplTokenSignKindSingle;
        BAIL_IF(instruction_accounts_iterator_next(it, &sign->single.signer));
    } else {
        sign->kind = SplTokenSignKindMulti;
        BAIL_IF(instruction_accounts_iterator_next(it, &sign->multi.account));
        BAIL_IF(parse_spl_token_multisigners(it, &sign->multi.signers))
    }
    return 0;
}

static int parse_transfer_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenTransferInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u64(parser, &info->body.amount));

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->src_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->dest_account));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_approve_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenApproveInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u64(parser, &info->body.amount));

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->delegate));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_revoke_spl_token_instruction(
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenRevokeInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_token_authority_type(
    Parser* parser,
    Token_AuthorityType* auth_type
) {
    uint8_t maybe_type;
    BAIL_IF(parse_u8(parser, &maybe_type));
    switch (maybe_type) {
        case Token_AuthorityType_MintTokens:
        case Token_AuthorityType_FreezeAccount:
        case Token_AuthorityType_AccountOwner:
        case Token_AuthorityType_CloseAccount:
            *auth_type = (Token_AuthorityType)maybe_type;
            return 0;
    }
    return 1;
}

static const char* stringify_token_authority_type(Token_AuthorityType auth_type) {
    switch (auth_type) {
        case Token_AuthorityType_MintTokens:
            return "Mint tokens";
        case Token_AuthorityType_FreezeAccount:
            return "Freeze account";
        case Token_AuthorityType_AccountOwner:
            return "Account owner";
        case Token_AuthorityType_CloseAccount:
            return "Close account";
    }
    return NULL;
}

static int parse_set_authority_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenSetAuthorityInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->account));

    BAIL_IF(parse_token_authority_type(parser, &info->authority_type));

    enum Option new_authority;
    BAIL_IF(parse_option(parser, &new_authority));
    if (new_authority == OptionSome) {
        BAIL_IF(parse_pubkey(parser, &info->new_authority));
    } else {
        info->new_authority = NULL;
    }

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_mint_to_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenMintToInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u64(parser, &info->body.amount));

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->mint_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_burn_spl_token_instruction(
    Parser* parser,
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenBurnInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(parse_u64(parser, &info->body.amount));

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

static int parse_close_account_spl_token_instruction(
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenCloseAccountInfo* info
) {
    InstructionAccountsIterator it;
    instruction_accounts_iterator_init(&it, header, instruction);

    BAIL_IF(instruction_accounts_iterator_next(&it, &info->token_account));
    BAIL_IF(instruction_accounts_iterator_next(&it, &info->dest_account));

    BAIL_IF(parse_spl_token_sign(&it, &info->sign));

    return 0;
}

int parse_spl_token_instructions(
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenInfo* info
) {
    Parser parser = {instruction->data, instruction->data_length};

    BAIL_IF(parse_spl_token_instruction_kind(&parser, &info->kind));

    switch (info->kind) {
        case SplTokenKind(InitializeMint):
            return parse_initialize_mint_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->initialize_mint
            );
        case SplTokenKind(InitializeAccount):
            return parse_initialize_account_spl_token_instruction(
                instruction,
                header,
                &info->initialize_account
            );
        case SplTokenKind(InitializeMultisig):
            return parse_initialize_multisig_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->initialize_multisig
            );
        case SplTokenKind(Transfer):
            return parse_transfer_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->transfer
            );
        case SplTokenKind(Approve):
            return parse_approve_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->approve
            );
        case SplTokenKind(Revoke):
            return parse_revoke_spl_token_instruction(
                instruction,
                header,
                &info->revoke
            );
        case SplTokenKind(SetAuthority):
            return parse_set_authority_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->set_owner
            );
        case SplTokenKind(MintTo):
            return parse_mint_to_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->mint_to
            );
        case SplTokenKind(Burn):
            return parse_burn_spl_token_instruction(
                &parser,
                instruction,
                header,
                &info->burn
            );
        case SplTokenKind(CloseAccount):
            return parse_close_account_spl_token_instruction(
                instruction,
                header,
                &info->close_account
            );
        case SplTokenKind(FreezeAccount):
        case SplTokenKind(ThawAccount):
        case SplTokenKind(Transfer2):
        case SplTokenKind(Approve2):
        case SplTokenKind(MintTo2):
        case SplTokenKind(Burn2):
            break;
    }
    return 1;
}

static int print_spl_token_sign(const SplTokenSign* sign) {
    SummaryItem* item;

    item = transaction_summary_general_item();
    if (sign->kind == SplTokenSignKindSingle) {
        summary_item_set_pubkey(item, "Owner", sign->single.signer);
    } else {
        summary_item_set_pubkey(item, "Owner", sign->multi.account);
        item = transaction_summary_general_item();
        summary_item_set_u64(item, "Signers", sign->multi.signers.count);
    }

    return 0;
}

static int print_spl_token_initialize_mint_info(
    const char* primary_title,
    const SplTokenInitializeMintInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    if (primary_title != NULL) {
        item = transaction_summary_primary_item();
        summary_item_set_pubkey(item, primary_title, info->mint_account);
    }

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Mint authority", info->mint_authority);

    item = transaction_summary_general_item();
    summary_item_set_u64(item, "Decimals", info->decimals);

    if (info->freeze_authority != NULL) {
        item = transaction_summary_general_item();
        summary_item_set_pubkey(item, "Freeze authority", info->freeze_authority);
    }

    return 0;
}

static int print_spl_token_initialize_account_info(
    const char* primary_title,
    const SplTokenInitializeAccountInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    if (primary_title != NULL) {
        item = transaction_summary_primary_item();
        summary_item_set_pubkey(item, primary_title, info->token_account);
    }

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Token account owner", info->owner);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Token mint", info->mint_account);

    return 0;
}

static int print_spl_token_initialize_multisig_info(
    const char* primary_title,
    const SplTokenInitializeMultisigInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    if (primary_title != NULL) {
        item = transaction_summary_primary_item();
        summary_item_set_pubkey(item, primary_title, info->multisig_account);
    }

    item = transaction_summary_general_item();
    summary_item_set_multisig_m_of_n(item, info->body.m, info->signers.count);

    return 0;
}

static int print_spl_token_transfer_info(
    const SplTokenTransferInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_u64(item, "Transfer tokens", info->body.amount);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "From", info->src_account);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "To", info->dest_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_approve_info(
    const SplTokenApproveInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Approve delegate", info->delegate);

    item = transaction_summary_general_item();
    summary_item_set_u64(item, "To spend", info->body.amount);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "From", info->token_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_revoke_info(
    const SplTokenRevokeInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Revoke delegate", info->token_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_set_authority_info(
    const SplTokenSetAuthorityInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Set authority", info->account);

    const char* authority_type = stringify_token_authority_type(info->authority_type);
    BAIL_IF(authority_type == NULL);
    item = transaction_summary_general_item();
    summary_item_set_string(item, "Type", authority_type);

    item = transaction_summary_general_item();
    const char* new_auth_title = "Authority";
    if (info->new_authority == NULL) {
        summary_item_set_string(item, new_auth_title, "None");
    } else {
        summary_item_set_pubkey(item, new_auth_title, info->new_authority);
    }

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_mint_to_info(
    const SplTokenMintToInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_u64(item, "Mint tokens", info->body.amount);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "From mint", info->mint_account);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "To account", info->token_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_burn_info(
    const SplTokenBurnInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_u64(item, "Burn tokens", info->body.amount);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "From account", info->token_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

static int print_spl_token_close_account_info(
    const SplTokenCloseAccountInfo* info,
    const MessageHeader* header
) {
    SummaryItem* item;

    item = transaction_summary_primary_item();
    summary_item_set_pubkey(item, "Close token acct", info->token_account);

    item = transaction_summary_general_item();
    summary_item_set_pubkey(item, "Withdraw to", info->dest_account);

    print_spl_token_sign(&info->sign);

    return 0;
}

int print_spl_token_info(
    const SplTokenInfo* info,
    const MessageHeader* header
) {
    switch (info->kind) {
        case SplTokenKind(InitializeMint):
            return print_spl_token_initialize_mint_info(
                "Init token mint",
                &info->initialize_mint,
                header
            );
        case SplTokenKind(InitializeAccount):
            return print_spl_token_initialize_account_info(
                "Init token acct",
                &info->initialize_account,
                header
            );
        case SplTokenKind(InitializeMultisig):
            return print_spl_token_initialize_multisig_info(
                "Init token multisig",
                &info->initialize_multisig,
                header
            );
        case SplTokenKind(Transfer):
            return print_spl_token_transfer_info(&info->transfer, header);
        case SplTokenKind(Approve):
            return print_spl_token_approve_info(&info->approve, header);
        case SplTokenKind(Revoke):
            return print_spl_token_revoke_info(&info->revoke, header);
        case SplTokenKind(SetAuthority):
            return print_spl_token_set_authority_info(&info->set_owner, header);
        case SplTokenKind(MintTo):
            return print_spl_token_mint_to_info(&info->mint_to, header);
        case SplTokenKind(Burn):
            return print_spl_token_burn_info(&info->burn, header);
        case SplTokenKind(CloseAccount):
            return print_spl_token_close_account_info(&info->close_account, header);
        case SplTokenKind(FreezeAccount):
        case SplTokenKind(ThawAccount):
        case SplTokenKind(Transfer2):
        case SplTokenKind(Approve2):
        case SplTokenKind(MintTo2):
        case SplTokenKind(Burn2):
            break;
    }

    return 1;
}

#define M_OF_N_MAX_LEN 9 // "11 of 11" + NUL
static int print_m_of_n_string(uint8_t m, uint8_t n, char* buf, size_t buflen) {
    BAIL_IF(n > Token_MAX_SIGNERS);
    BAIL_IF(m > n);
    BAIL_IF(buflen < M_OF_N_MAX_LEN);

    size_t i = 0;
    if (m > 9) buf[i++] = '1';
    buf[i++] = '0' + (m % 10);
    strncpy(&buf[i], " of ", 5);
    i += 4;
    if (n > 9) buf[i++] = '1';
    buf[i++] = '0' + (n % 10);
    buf[i] = '\0';

    return 0;
}

void summary_item_set_multisig_m_of_n(SummaryItem* item, uint8_t m, uint8_t n) {
    static char m_of_n[M_OF_N_MAX_LEN];

    if (print_m_of_n_string(m, n, m_of_n, sizeof(m_of_n)) == 0) {
        summary_item_set_string(item, "Required signers", m_of_n);
    }
}

const Pubkey* spl_token_option_pubkey_get(
    const SplTokenOptionPubkey* option_pubkey
) {
    switch (option_pubkey->tag) {
        case SplTokenToOptionPubkeyKind(None):
            break;
        case SplTokenToOptionPubkeyKind(Some):
            return (const Pubkey*)&option_pubkey->some._0;
    }
    return NULL;
}
