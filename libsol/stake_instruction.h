#pragma once

#include "sol/parser.h"

extern const Pubkey stake_program_id;

enum StakeInstructionKind {
    Initialize,
    Authorize,
    DelegateStake,
    Split,
    Withdraw,
    Deactivate,
    SetLockup,
};

typedef struct DelegateStakeInfo {
    Pubkey* stake_pubkey;
    Pubkey* vote_pubkey;
    Pubkey* authorized_pubkey;
} DelegateStakeInfo;

int parse_delegate_stake_instructions(Parser* parser, Instruction* instruction, MessageHeader* header, DelegateStakeInfo* info);
int print_delegate_stake_info(DelegateStakeInfo* info, MessageHeader* header, field_t* fields, size_t* fields_used);
