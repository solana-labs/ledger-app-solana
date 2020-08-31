#pragma once

#include "sol/transaction_summary.h"
#include "spl/token.h"

#define SplTokenBody(b) Token_TokenInstruction_Token_ ## b ## _Body
#define SplTokenKind(b) Token_TokenInstruction_ ## b

#define SplTokenInstructionKind Token_TokenInstruction_Tag

typedef struct SplTokenMultisigners {
    const Pubkey* first;
    uint8_t count;
} SplTokenMultisigners;

typedef enum {
    SplTokenSignKindSingle,
    SplTokenSignKindMulti,
} SplTokenSignKind;

typedef struct {
    SplTokenSignKind kind;
    union {
        struct {
            const Pubkey* signer;
        } single;
        struct {
            const Pubkey* account;
            SplTokenMultisigners signers;
        } multi;
    };
} SplTokenSign;

extern const Pubkey spl_token_program_id;

typedef struct SplTokenInitializeMintInfo {
    const Pubkey* mint_account;
    const Pubkey* mint_authority;
    const Pubkey* freeze_authority;
    uint8_t decimals;
} SplTokenInitializeMintInfo;

typedef struct SplTokenInitializeAccountInfo {
    const Pubkey* token_account;
    const Pubkey* mint_account;
    const Pubkey* owner;
} SplTokenInitializeAccountInfo;

typedef struct SplTokenInitializeMultisigInfo {
    const Pubkey* multisig_account;
    SplTokenMultisigners signers;
    SplTokenBody(InitializeMultisig) body;
} SplTokenInitializeMultisigInfo;

typedef struct SplTokenTransferInfo {
    const Pubkey* src_account;
    const Pubkey* dest_account;
    SplTokenSign sign;
    SplTokenBody(Transfer) body;
} SplTokenTransferInfo;

typedef struct SplTokenApproveInfo {
    const Pubkey* token_account;
    const Pubkey* delegate;
    SplTokenSign sign;
    SplTokenBody(Approve) body;
} SplTokenApproveInfo;

typedef struct SplTokenRevokeInfo {
    const Pubkey* token_account;
    SplTokenSign sign;
} SplTokenRevokeInfo;

typedef struct SplTokenSetAuthorityInfo {
    const Pubkey* account;
    const Pubkey* new_authority;
    Token_AuthorityType authority_type;
    SplTokenSign sign;
} SplTokenSetAuthorityInfo;

typedef struct SplTokenMintToInfo {
    const Pubkey* mint_account;
    const Pubkey* token_account;
    SplTokenSign sign;
    SplTokenBody(MintTo) body;
} SplTokenMintToInfo;

typedef struct SplTokenBurnInfo {
    const Pubkey* token_account;
    SplTokenSign sign;
    SplTokenBody(Burn) body;
} SplTokenBurnInfo;

typedef struct SplTokenCloseAccountInfo {
    const Pubkey* token_account;
    const Pubkey* dest_account;
    SplTokenSign sign;
} SplTokenCloseAccountInfo;

typedef struct SplTokenInfo {
    SplTokenInstructionKind kind;
    union {
        SplTokenInitializeMintInfo initialize_mint;
        SplTokenInitializeAccountInfo initialize_account;
        SplTokenInitializeMultisigInfo initialize_multisig;
        SplTokenTransferInfo transfer;
        SplTokenApproveInfo approve;
        SplTokenRevokeInfo revoke;
        SplTokenSetAuthorityInfo set_owner;
        SplTokenMintToInfo mint_to;
        SplTokenBurnInfo burn;
        SplTokenCloseAccountInfo close_account;
    };
} SplTokenInfo;

int parse_spl_token_instructions(
    const Instruction* instruction,
    const MessageHeader* header,
    SplTokenInfo* info
);
int print_spl_token_info(
    const SplTokenInfo* info,
    const MessageHeader* header
);
void summary_item_set_multisig_m_of_n(
    SummaryItem* item,
    uint8_t m,
    uint8_t n
);

#define SplTokenOptionPubkeyKind Token_COption_Pubkey_Tag
#define SplTokenToOptionPubkeyKind(k) Token_COption_Pubkey_ ## k ## _Pubkey
#define SplTokenOptionPubkeyBody Token_COption_Pubkey_Token_Some_Body_Pubkey
#define SplTokenOptionPubkey Token_COption_Pubkey
const Pubkey* spl_token_option_pubkey_get(
    const SplTokenOptionPubkey* option_pubkey
);

