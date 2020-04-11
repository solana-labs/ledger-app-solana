#pragma once

#include "sol/parser.h"

extern const Pubkey vote_program_id;

enum VoteInstructionKind {
    VoteInitialize,
    VoteAuthorize,
    VoteVote,
    VoteWithdraw,
    VoteUpdateNode,
};

typedef struct VoteInitData {
    const Pubkey* node;
    const Pubkey* vote_authority;
    const Pubkey* withdraw_authority;
    uint64_t commission;
} VoteInitData;

typedef struct VoteInitializeInfo {
    const Pubkey* account;
    VoteInitData vote_init;
} VoteInitializeInfo;

typedef struct VoteInfo {
    enum VoteInstructionKind kind;
    union {
        VoteInitializeInfo initialize;
    };
} VoteInfo;

int parse_vote_instructions(const Instruction* instruction, const MessageHeader* header, VoteInfo* info);
int print_vote_info(const VoteInfo* info, const MessageHeader* header);
int print_vote_initialize_info(
    const char* primary_title,
    const VoteInitializeInfo* info,
    const MessageHeader* header
);
