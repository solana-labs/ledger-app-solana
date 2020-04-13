#pragma once

#include "sol/parser.h"

extern const Pubkey system_program_id;

enum SystemInstructionKind {
    SystemCreateAccount,
    SystemAssign,
    SystemTransfer,
    SystemCreateAccountWithSeed,
    SystemAdvanceNonceAccount,
    SystemWithdrawNonceAccount,
    SystemInitializeNonceAccount,
    SystemAuthorizeNonceAccount,
    SystemAllocate,
    SystemAllocateWithSeed,
    SystemAssignWithSeed
};

typedef struct SystemCreateAccountInfo {
    const Pubkey* from;
    const Pubkey* to;
    uint64_t lamports;
} SystemCreateAccountInfo;

typedef struct SystemCreateAccountWithSeedInfo {
    const Pubkey* from;
    const Pubkey* to;
    const Pubkey* base;
    SizedString seed;
    uint64_t lamports;
} SystemCreateAccountWithSeedInfo;

typedef struct SystemTransferInfo {
    const Pubkey* from;
    const Pubkey* to;
    uint64_t lamports;
} SystemTransferInfo;

typedef struct SystemAdvanceNonceInfo {
    const Pubkey* account;
    const Pubkey* authority;
} SystemAdvanceNonceInfo;

typedef struct SystemInitializeNonceInfo {
    const Pubkey* account;
    const Pubkey* authority;
} SystemInitializeNonceInfo;

typedef struct SystemWithdrawNonceInfo {
    const Pubkey* account;
    const Pubkey* authority;
    const Pubkey* to;
    uint64_t lamports;
} SystemWithdrawNonceInfo;

typedef struct SystemInfo {
    enum SystemInstructionKind kind;
    union {
        SystemTransferInfo transfer;
        SystemCreateAccountInfo create_account;
        SystemCreateAccountWithSeedInfo create_account_with_seed;
        SystemAdvanceNonceInfo advance_nonce;
        SystemInitializeNonceInfo initialize_nonce;
        SystemWithdrawNonceInfo withdraw_nonce;
    };
} SystemInfo;

int parse_system_instructions(const Instruction* instruction, const MessageHeader* header, SystemInfo* info);
int print_system_info(const SystemInfo* info, const MessageHeader* header);
int print_system_nonced_transaction_sentinel(const SystemInfo* info, const MessageHeader* header);
int print_system_create_account_info(
    const char* primary_title,
    const SystemCreateAccountInfo* info,
    const MessageHeader* header
);
int print_system_create_account_with_seed_info(
    const char* primary_title,
    const SystemCreateAccountWithSeedInfo* info,
    const MessageHeader* header
);
int print_system_initialize_nonce_info(
    const char* primary_title,
    const SystemInitializeNonceInfo* info,
    const MessageHeader* header
);
