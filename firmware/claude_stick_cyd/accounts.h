#pragma once
#include <Preferences.h>
#include "crypto.h"

#define ACCT_MAX      4
#define ACCT_LBL_MAX  17

struct AccountSlots {
    bool used[ACCT_MAX];
    char label[ACCT_MAX][ACCT_LBL_MAX];
    int  active;
};

void accountsLoad(Preferences& p, AccountSlots& s);
bool accountLoadBlob(Preferences& p, int slot, EncryptedBlob& out);
void accountSave(Preferences& p, AccountSlots& s, int slot,
                 const EncryptedBlob& blob, const char* label);
void accountSetLabel(Preferences& p, AccountSlots& s, int slot, const char* label);
void accountRemove(Preferences& p, AccountSlots& s, int slot);
void accountSetActive(Preferences& p, AccountSlots& s, int slot);
int  accountCount(const AccountSlots& s);
int  accountFirstFree(const AccountSlots& s);
