// Register.h — DLL registration and IME profile management
#pragma once
#include "Globals.h"

BOOL RegisterServer();
BOOL UnregisterServer();
BOOL RegisterProfiles();
BOOL UnregisterProfiles();
BOOL RegisterCategories();
BOOL UnregisterCategories();
