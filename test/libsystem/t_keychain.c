/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Security/Security.h>
#include <string.h>
#include "check.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

int
main(void)
{
    SecKeychainRef keychain = (SecKeychainRef)(uintptr_t)1;
    SecKeychainAttribute attrs[2];
    SecKeychainAttributeList list = { 2, attrs };
    UInt32 length = 0;
    void *password = (void *)1;

    CHECK(SecKeychainCopyDefault(&keychain) == errSecNoDefaultKeychain && keychain == NULL);
    keychain = (SecKeychainRef)(uintptr_t)1;
    CHECK(SecKeychainCopyDomainDefault(kSecPreferencesDomainUser, &keychain) == errSecNoDefaultKeychain);
    CHECK(keychain == NULL);
    CHECK(SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem, &keychain) == errSecNoDefaultKeychain);
    CHECK(SecKeychainCopyDomainDefault(kSecPreferencesDomainDynamic, &keychain) == errSecInvalidPrefsDomain);
    SecKeychainRef *volatile none = NULL;
    CHECK(SecKeychainCopyDefault(none) == errSecParam);

    CHECK(SecKeychainFindGenericPassword(NULL, 4, "kido", 4, "test", &length, &password, NULL) == errSecItemNotFound);
    CHECK(password == NULL && length == 0);
    CHECK(SecKeychainItemFreeContent(NULL, password) == errSecSuccess);

    attrs[0].tag = kSecAccountItemAttr;
    attrs[0].length = 4;
    attrs[0].data = strdup("kido");
    attrs[1].tag = kSecServiceItemAttr;
    attrs[1].length = 4;
    attrs[1].data = strdup("test");
    CHECK(SecKeychainItemFreeContent(&list, malloc(16)) == errSecSuccess);
    CHECK(attrs[0].data == NULL && attrs[1].data == NULL && list.count == 2);

    return checks_done("keychain");
}
