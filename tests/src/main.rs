use rand::prelude::{RngCore, SeedableRng, StdRng};
use solana_remote_wallet::ledger::LedgerWallet;
use solana_remote_wallet::remote_wallet::{
    initialize_wallet_manager, DerivationPath, RemoteWallet,
};
use solana_sdk::{
    instruction::{AccountMeta, Instruction},
    message::Message,
    pubkey::Pubkey,
    system_instruction, system_program,
};
use solana_stake_program::{stake_instruction, stake_state};
use solana_vote_program::{vote_instruction, vote_state};
use std::collections::HashSet;
use std::sync::Arc;

fn get_ledger() -> (Arc<LedgerWallet>, Pubkey) {
    let wallet_manager = initialize_wallet_manager().expect("Couldn't start wallet manager");

    // Update device list
    const NO_DEVICE_HELP: &str = "No Ledger found, make sure you have a unlocked Ledger connected with the Ledger Wallet Solana running";
    wallet_manager.update_devices().expect(NO_DEVICE_HELP);
    assert!(!wallet_manager.list_devices().is_empty(), NO_DEVICE_HELP);

    // Fetch the base pubkey of a connected ledger device
    let ledger_base_pubkey = wallet_manager
        .list_devices()
        .iter()
        .find(|d| d.manufacturer == "ledger")
        .map(|d| d.pubkey)
        .expect("No ledger device detected");

    let ledger = wallet_manager
        .get_ledger(&ledger_base_pubkey)
        .expect("get device");

    (ledger, ledger_base_pubkey)
}

fn test_ledger_pubkey() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let mut pubkey_set = HashSet::new();
    pubkey_set.insert(ledger_base_pubkey);

    let pubkey_0_0 = ledger
        .get_pubkey(
            &DerivationPath {
                account: Some(0.into()),
                change: Some(0.into()),
            },
            false,
        )
        .expect("get pubkey");
    pubkey_set.insert(pubkey_0_0);
    let pubkey_0_1 = ledger
        .get_pubkey(
            &DerivationPath {
                account: Some(0.into()),
                change: Some(1.into()),
            },
            false,
        )
        .expect("get pubkey");
    pubkey_set.insert(pubkey_0_1);
    let pubkey_1 = ledger
        .get_pubkey(
            &DerivationPath {
                account: Some(1.into()),
                change: None,
            },
            false,
        )
        .expect("get pubkey");
    pubkey_set.insert(pubkey_1);
    let pubkey_1_0 = ledger
        .get_pubkey(
            &DerivationPath {
                account: Some(1.into()),
                change: Some(0.into()),
            },
            false,
        )
        .expect("get pubkey");
    pubkey_set.insert(pubkey_1_0);

    assert_eq!(pubkey_set.len(), 5); // Ensure keys at various derivation paths are unique
}

// This test requires interactive approval of message signing on the ledger.
fn test_ledger_sign_transaction() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let instruction = system_instruction::transfer(&from, &ledger_base_pubkey, 42);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));

    // Test large transaction
    let recipients: Vec<(Pubkey, u64)> = (0..10).map(|_| (Pubkey::new_rand(), 42)).collect();
    let instructions = system_instruction::transfer_many(&from, &recipients);
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

fn test_ledger_sign_transaction_too_big() {
    // Test too big of a transaction
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let recipients: Vec<(Pubkey, u64)> = (0..100).map(|_| (Pubkey::new_rand(), 42)).collect();
    let instructions = system_instruction::transfer_many(&from, &recipients);
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    ledger.sign_message(&derivation_path, &message).unwrap_err();
}

/// This test requires interactive approval of message signing on the ledger.
fn test_ledger_delegate_stake() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let authorized_pubkey = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let stake_pubkey = ledger_base_pubkey;
    let vote_pubkey = Pubkey::default();
    let instruction =
        stake_instruction::delegate_stake(&stake_pubkey, &authorized_pubkey, &vote_pubkey);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&authorized_pubkey.as_ref(), &message));
}

/// This test requires interactive approval of message signing on the ledger.
fn test_ledger_delegate_stake_with_nonce() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let authorized_pubkey = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let stake_pubkey = ledger_base_pubkey;
    let vote_pubkey = Pubkey::new(&[1u8; 32]);
    let instruction =
        stake_instruction::delegate_stake(&stake_pubkey, &authorized_pubkey, &vote_pubkey);
    let nonce_account = Pubkey::new(&[2u8; 32]);
    let message =
        Message::new_with_nonce(vec![instruction], None, &nonce_account, &authorized_pubkey)
            .serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&authorized_pubkey.as_ref(), &message));
}

/// This test requires interactive approval of message signing on the ledger.
fn test_ledger_advance_nonce_account() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let authorized_pubkey = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let nonce_account = Pubkey::new(&[1u8; 32]);
    let instruction = system_instruction::advance_nonce_account(&nonce_account, &authorized_pubkey);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&authorized_pubkey.as_ref(), &message));
}

/// This test requires interactive approval of message signing on the ledger.
fn test_ledger_advance_nonce_account_separate_fee_payer() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let authorized_pubkey = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let nonce_account = Pubkey::new(&[1u8; 32]);
    let fee_payer = Pubkey::new(&[2u8; 32]);
    let instruction = system_instruction::advance_nonce_account(&nonce_account, &authorized_pubkey);
    let message = Message::new(&[instruction], Some(&fee_payer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&authorized_pubkey.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_ledger_transfer_with_nonce() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let nonce_account = Pubkey::new(&[1u8; 32]);
    let nonce_authority = Pubkey::new(&[2u8; 32]);
    let instruction = system_instruction::transfer(&from, &ledger_base_pubkey, 42);
    let message =
        Message::new_with_nonce(vec![instruction], None, &nonce_account, &nonce_authority)
            .serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_stake_account_with_seed_and_nonce() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let nonce_account = Pubkey::new(&[1u8; 32]);
    let nonce_authority = Pubkey::new(&[2u8; 32]);
    let base = from;
    let seed = "seedseedseedseedseedseedseedseed";
    let stake_account = Pubkey::create_with_seed(&base, seed, &solana_stake_program::id()).unwrap();
    let authorized = stake_state::Authorized {
        staker: Pubkey::new(&[3u8; 32]),
        withdrawer: Pubkey::new(&[4u8; 32]),
    };
    let instructions = stake_instruction::create_account_with_seed(
        &from,
        &stake_account,
        &base,
        seed,
        &authorized,
        &stake_state::Lockup::default(),
        42,
    );
    let message =
        Message::new_with_nonce(instructions, None, &nonce_account, &nonce_authority).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_stake_account() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let stake_account = ledger_base_pubkey;
    let authorized = stake_state::Authorized {
        staker: Pubkey::new(&[3u8; 32]),
        withdrawer: Pubkey::new(&[4u8; 32]),
    };
    let instructions = stake_instruction::create_account(
        &from,
        &stake_account,
        &authorized,
        &stake_state::Lockup::default(),
        42,
    );
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_nonce_account_with_seed() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let base = from;
    let seed = "seedseedseedseedseedseedseedseed";
    let nonce_account = Pubkey::create_with_seed(&base, seed, &system_program::id()).unwrap();
    let instructions = system_instruction::create_nonce_account_with_seed(
        &from,
        &nonce_account,
        &base,
        seed,
        &Pubkey::new(&[1u8; 32]),
        42,
    );
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_nonce_account() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let nonce_account = ledger_base_pubkey;
    let instructions = system_instruction::create_nonce_account(
        &from,
        &nonce_account,
        &Pubkey::new(&[1u8; 32]),
        42,
    );
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_sign_full_shred_of_garbage_tx() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");

    let program_id = Pubkey::new(&[1u8; 32]);
    let mut data = [0u8; 1232 - 106].to_vec();
    let mut rng = StdRng::seed_from_u64(0);
    rng.fill_bytes(&mut data);
    let instruction = Instruction {
        program_id,
        accounts: Vec::from([AccountMeta::new_readonly(from, true)]),
        data,
    };
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_vote_account() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let vote_account = ledger_base_pubkey;
    let vote_init = vote_state::VoteInit {
        node_pubkey: Pubkey::new(&[1u8; 32]),
        authorized_voter: Pubkey::new(&[2u8; 32]),
        authorized_withdrawer: Pubkey::new(&[3u8; 32]),
        commission: 50u8,
    };
    let instructions = vote_instruction::create_account(&from, &vote_account, &vote_init, 42);
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_create_vote_account_with_seed() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let base = from;
    let seed = "seedseedseedseedseedseedseedseed";
    let vote_account = Pubkey::create_with_seed(&base, seed, &solana_vote_program::id()).unwrap();
    let vote_init = vote_state::VoteInit {
        node_pubkey: Pubkey::new(&[1u8; 32]),
        authorized_voter: Pubkey::new(&[2u8; 32]),
        authorized_withdrawer: Pubkey::new(&[3u8; 32]),
        commission: 50u8,
    };
    let instructions = vote_instruction::create_account_with_seed(
        &from,
        &vote_account,
        &base,
        seed,
        &vote_init,
        42,
    );
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&from.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_nonce_withdraw() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let nonce_account = ledger_base_pubkey;
    let nonce_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let to = Pubkey::new(&[1u8; 32]);
    let instruction =
        system_instruction::withdraw_nonce_account(&nonce_account, &nonce_authority, &to, 42);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&nonce_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_stake_withdraw() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_account = ledger_base_pubkey;
    let stake_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let to = Pubkey::new(&[1u8; 32]);
    let instruction = stake_instruction::withdraw(&stake_account, &stake_authority, &to, 42, None);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_vote_withdraw() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let vote_account = ledger_base_pubkey;
    let vote_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let to = Pubkey::new(&[1u8; 32]);
    let instruction = vote_instruction::withdraw(&vote_account, &vote_authority, 42, &to);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&vote_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_nonce_authorize() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let nonce_account = ledger_base_pubkey;
    let nonce_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let new_authority = Pubkey::new(&[1u8; 32]);
    let instruction = system_instruction::authorize_nonce_account(
        &nonce_account,
        &nonce_authority,
        &new_authority,
    );
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&nonce_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_stake_authorize() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_account = ledger_base_pubkey;
    let stake_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let new_authority = Pubkey::new(&[1u8; 32]);
    let stake_auth = stake_instruction::authorize(
        &stake_account,
        &stake_authority,
        &new_authority,
        stake_state::StakeAuthorize::Staker,
    );

    // Authorize staker
    let message = Message::new(&[stake_auth.clone()], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));

    let new_authority = Pubkey::new(&[2u8; 32]);
    let withdraw_auth = stake_instruction::authorize(
        &stake_account,
        &stake_authority,
        &new_authority,
        stake_state::StakeAuthorize::Withdrawer,
    );

    // Authorize withdrawer
    let message = Message::new(&[withdraw_auth.clone()], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));

    // Authorize both
    // Note: Instruction order must match CLI; staker first, withdrawer second
    let message = Message::new(&[stake_auth, withdraw_auth], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_vote_authorize() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let vote_account = ledger_base_pubkey;
    let vote_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let new_authority = Pubkey::new(&[1u8; 32]);
    let vote_auth = vote_instruction::authorize(
        &vote_account,
        &vote_authority,
        &new_authority,
        vote_state::VoteAuthorize::Voter,
    );

    // Authorize voter
    let message = Message::new(&[vote_auth.clone()], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&vote_authority.as_ref(), &message));

    let new_authority = Pubkey::new(&[2u8; 32]);
    let withdraw_auth = vote_instruction::authorize(
        &vote_account,
        &vote_authority,
        &new_authority,
        vote_state::VoteAuthorize::Withdrawer,
    );

    // Authorize withdrawer
    let message = Message::new(&[withdraw_auth.clone()], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&vote_authority.as_ref(), &message));

    // Authorize both
    // Note: Instruction order must match CLI; voter first, withdrawer second
    let message = Message::new(&[vote_auth, withdraw_auth], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&vote_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_vote_update_validator_identity() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let vote_account = ledger_base_pubkey;
    let vote_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let new_node = Pubkey::new(&[1u8; 32]);
    let instruction =
        vote_instruction::update_validator_identity(&vote_account, &vote_authority, &new_node);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&vote_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_stake_deactivate() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_account = ledger_base_pubkey;
    let stake_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let instruction = stake_instruction::deactivate_stake(&stake_account, &stake_authority);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_stake_set_lockup() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_account = ledger_base_pubkey;
    let stake_custodian = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let new_custodian = Pubkey::new(&[1u8; 32]);
    let lockup = stake_instruction::LockupArgs {
        unix_timestamp: Some(1),
        epoch: Some(2),
        custodian: Some(new_custodian),
    };
    let instruction = stake_instruction::set_lockup(&stake_account, &lockup, &stake_custodian);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_custodian.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
// Add a nonce here to exercise worst case instruction usage
fn test_stake_split_with_nonce() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let stake_account = ledger_base_pubkey;
    let split_account = Pubkey::new(&[1u8; 32]);
    let nonce_account = Pubkey::new(&[2u8; 32]);
    let nonce_authority = Pubkey::new(&[3u8; 32]);
    let instructions =
        stake_instruction::split(&stake_account, &stake_authority, 42, &split_account);
    let message =
        Message::new_with_nonce(instructions, None, &nonce_account, &nonce_authority).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));
}

// This test requires interactive approval of message signing on the ledger.
fn test_stake_split_with_seed() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let stake_authority = ledger
        .get_pubkey(&derivation_path, false)
        .expect("get pubkey");
    let stake_account = ledger_base_pubkey;
    let base = stake_authority;
    let seed = "seedseedseedseedseedseedseedseed";
    let split_account = Pubkey::create_with_seed(&base, seed, &solana_stake_program::id()).unwrap();
    let instructions = stake_instruction::split_with_seed(
        &stake_account,
        &stake_authority,
        42,
        &split_account,
        &base,
        seed,
    );
    let message = Message::new(&instructions, Some(&ledger_base_pubkey)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&stake_authority.as_ref(), &message));
}

fn test_ledger_reject_unexpected_signer() {
    let (ledger, ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };

    let from = Pubkey::new(&[1u8; 32]);
    let instruction = system_instruction::transfer(&from, &ledger_base_pubkey, 42);
    let message = Message::new(&[instruction], Some(&ledger_base_pubkey)).serialize();
    assert!(ledger.sign_message(&derivation_path, &message).is_err());
}

fn test_spl_token_create_mint() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let mint = Pubkey::new(&[1u8; 32]);

    let instructions = vec![
        system_instruction::create_account(
            &owner,
            &mint,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_mint(&spl_token::id(), &mint, &owner, None, 2).unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_create_account() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let mint = Pubkey::new(&[2u8; 32]);

    let instructions = vec![
        system_instruction::create_account(
            &owner,
            &account,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_account(&spl_token::id(), &account, &mint, &owner)
            .unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_create_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let signers = [
        Pubkey::new(&[2u8; 32]),
        Pubkey::new(&[3u8; 32]),
        Pubkey::new(&[4u8; 32]),
    ];

    let instructions = vec![
        system_instruction::create_account(
            &owner,
            &account,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_multisig(
            &spl_token::id(),
            &account,
            &signers.iter().collect::<Vec<_>>(),
            2,
        )
        .unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_create_mint_with_seed() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let base = owner;
    let seed = "seedseedseedseedseedseedseedseed";
    let mint = Pubkey::create_with_seed(&base, seed, &spl_token::id()).unwrap();

    let instructions = vec![
        system_instruction::create_account_with_seed(
            &owner,
            &mint,
            &base,
            &seed,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_mint(&spl_token::id(), &mint, &owner, None, 2).unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_create_account_with_seed() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let base = owner;
    let seed = "seedseedseedseedseedseedseedseed";
    let mint = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::create_with_seed(&base, seed, &spl_token::id()).unwrap();

    let instructions = vec![
        system_instruction::create_account_with_seed(
            &owner,
            &mint,
            &base,
            &seed,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_account(&spl_token::id(), &account, &mint, &owner)
            .unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_create_multisig_with_seed() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let base = owner;
    let seed = "seedseedseedseedseedseedseedseed";
    let mint = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::create_with_seed(&base, seed, &spl_token::id()).unwrap();
    let signers = [
        Pubkey::new(&[2u8; 32]),
        Pubkey::new(&[3u8; 32]),
        Pubkey::new(&[4u8; 32]),
    ];

    let instructions = vec![
        system_instruction::create_account_with_seed(
            &owner,
            &mint,
            &base,
            &seed,
            501,
            std::mem::size_of::<spl_token::state::Mint>() as u64,
            &spl_token::id(),
        ),
        spl_token::instruction::initialize_multisig(
            &spl_token::id(),
            &account,
            &signers.iter().collect::<Vec<_>>(),
            2,
        )
        .unwrap(),
    ];
    let message = Message::new(&instructions, Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_transfer() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let sender = Pubkey::new(&[1u8; 32]);
    let recipient = Pubkey::new(&[2u8; 32]);

    let instruction =
        spl_token::instruction::transfer(&spl_token::id(), &sender, &recipient, &owner, &[], 42)
            .unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_approve() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let delegate = Pubkey::new(&[2u8; 32]);

    let instruction =
        spl_token::instruction::approve(&spl_token::id(), &account, &delegate, &owner, &[], 42)
            .unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_revoke() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);

    let instruction =
        spl_token::instruction::revoke(&spl_token::id(), &account, &owner, &[]).unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_set_authority() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let new_owner = Pubkey::new(&[2u8; 32]);

    let instruction = spl_token::instruction::set_authority(
        &spl_token::id(),
        &account,
        Some(&new_owner),
        spl_token::instruction::AuthorityType::AccountOwner,
        &owner,
        &[],
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    println!("{:?}", message);
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_mint_to() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let mint = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);

    let instruction =
        spl_token::instruction::mint_to(&spl_token::id(), &mint, &account, &owner, &[], 42)
            .unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_burn() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let mint = Pubkey::new(&[2u8; 32]);

    let instruction =
        spl_token::instruction::burn(&spl_token::id(), &account, &mint, &owner, &[], 42).unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_close_account() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let owner = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let account = Pubkey::new(&[1u8; 32]);
    let destination = Pubkey::new(&[2u8; 32]);

    let instruction = spl_token::instruction::close_account(
        &spl_token::id(),
        &account,
        &destination,
        &owner,
        &[],
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&owner)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&owner.as_ref(), &message));
}

fn test_spl_token_transfer_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let sender = Pubkey::new(&[2u8; 32]);
    let recipient = Pubkey::new(&[3u8; 32]);
    let signers = [Pubkey::new(&[4u8; 32]), signer];

    let instruction = spl_token::instruction::transfer(
        &spl_token::id(),
        &sender,
        &recipient,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
        42,
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_approve_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);
    let delegate = Pubkey::new(&[3u8; 32]);
    let signers = [Pubkey::new(&[4u8; 32]), signer];

    let instruction = spl_token::instruction::approve(
        &spl_token::id(),
        &account,
        &delegate,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
        42,
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_revoke_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);
    let signers = [Pubkey::new(&[3u8; 32]), signer];

    let instruction = spl_token::instruction::revoke(
        &spl_token::id(),
        &account,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_set_authority_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);
    let new_owner = Pubkey::new(&[3u8; 32]);
    let signers = [Pubkey::new(&[4u8; 32]), signer];

    let instruction = spl_token::instruction::set_authority(
        &spl_token::id(),
        &account,
        Some(&new_owner),
        spl_token::instruction::AuthorityType::AccountOwner,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_mint_to_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let mint = Pubkey::new(&[2u8; 32]);
    let account = Pubkey::new(&[3u8; 32]);
    let signers = [Pubkey::new(&[4u8; 32]), signer];

    let instruction = spl_token::instruction::mint_to(
        &spl_token::id(),
        &mint,
        &account,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
        42,
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_burn_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);
    let signers = [Pubkey::new(&[3u8; 32]), signer];
    let mint = Pubkey::new(&[4u8; 32]);

    let instruction = spl_token::instruction::burn(
        &spl_token::id(),
        &account,
        &mint,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
        42,
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

fn test_spl_token_close_account_multisig() {
    let (ledger, _ledger_base_pubkey) = get_ledger();

    let derivation_path = DerivationPath {
        account: Some(12345.into()),
        change: None,
    };
    let signer = ledger
        .get_pubkey(&derivation_path, false)
        .expect("ledger get pubkey");
    let owner = Pubkey::new(&[1u8; 32]);
    let account = Pubkey::new(&[2u8; 32]);
    let destination = Pubkey::new(&[3u8; 32]);
    let signers = [Pubkey::new(&[4u8; 32]), signer];

    let instruction = spl_token::instruction::close_account(
        &spl_token::id(),
        &account,
        &destination,
        &owner,
        &signers.iter().collect::<Vec<_>>(),
    )
    .unwrap();
    let message = Message::new(&[instruction], Some(&signer)).serialize();
    let signature = ledger
        .sign_message(&derivation_path, &message)
        .expect("sign transaction");
    assert!(signature.verify(&signer.as_ref(), &message));
}

macro_rules! run {
    ($test:ident) => {
        println!(" >>> Running {} <<<", stringify!($test));
        $test();
    };
}
fn main() {
    run!(test_spl_token_set_authority);
    run!(test_spl_token_set_authority_multisig);
    run!(test_spl_token_create_mint);
    run!(test_spl_token_create_mint_with_seed);
    run!(test_spl_token_transfer_multisig);
    run!(test_spl_token_approve_multisig);
    run!(test_spl_token_revoke_multisig);
    run!(test_spl_token_mint_to_multisig);
    run!(test_spl_token_burn_multisig);
    run!(test_spl_token_close_account_multisig);
    run!(test_spl_token_create_account_with_seed);
    run!(test_spl_token_create_multisig_with_seed);
    run!(test_spl_token_create_account);
    run!(test_spl_token_create_multisig);
    run!(test_spl_token_transfer);
    run!(test_spl_token_approve);
    run!(test_spl_token_revoke);
    run!(test_spl_token_mint_to);
    run!(test_spl_token_burn);
    run!(test_spl_token_close_account);
    run!(test_ledger_reject_unexpected_signer);
    run!(test_stake_split_with_nonce);
    run!(test_stake_split_with_seed);
    run!(test_stake_set_lockup);
    run!(test_stake_deactivate);
    run!(test_vote_update_validator_identity);
    run!(test_vote_authorize);
    run!(test_stake_authorize);
    run!(test_nonce_authorize);
    run!(test_vote_withdraw);
    run!(test_stake_withdraw);
    run!(test_nonce_withdraw);
    run!(test_create_vote_account);
    run!(test_create_vote_account_with_seed);
    run!(test_create_nonce_account);
    run!(test_create_nonce_account_with_seed);
    run!(test_create_stake_account);
    run!(test_ledger_pubkey);
    run!(test_ledger_sign_transaction);
    run!(test_ledger_sign_transaction_too_big);
    run!(test_ledger_delegate_stake);
    run!(test_ledger_advance_nonce_account);
    run!(test_ledger_advance_nonce_account_separate_fee_payer);
    run!(test_ledger_delegate_stake_with_nonce);
    run!(test_ledger_transfer_with_nonce);
    run!(test_create_stake_account_with_seed_and_nonce);
    run!(test_sign_full_shred_of_garbage_tx);
}
