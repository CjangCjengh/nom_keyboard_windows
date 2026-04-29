// Globals.h — Shared constants and GUIDs for Nom IME (Windows TSF)
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>
#include <olectl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <mutex>
#include <fstream>
#include <sstream>
#include <filesystem>

// ---- Module handle (set in DllMain) ----
extern HINSTANCE g_hInst;
extern LONG g_cRefDll;

// ---- Text Service GUID ----
// {7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}
static const CLSID CLSID_NomTextService =
{ 0x7a8b9c0d, 0x1e2f, 0x3a4b, { 0x5c, 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d } };

// ---- Profile GUID (language profile for Vietnamese) ----
// {8B9C0D1E-2F3A-4B5C-6D7E-8F9A0B1C2D3E}
static const GUID GUID_NomProfile =
{ 0x8b9c0d1e, 0x2f3a, 0x4b5c, { 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e } };

// ---- Compartment GUID for open/close ----
// {9C0D1E2F-3A4B-5C6D-7E8F-9A0B1C2D3E4F}
static const GUID GUID_NomCompartment =
{ 0x9c0d1e2f, 0x3a4b, 0x5c6d, { 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f } };

// ---- Display attribute GUIDs ----
// {AD1E2F3A-4B5C-6D7E-8F9A-0B1C2D3E4F50}
static const GUID GUID_NomDisplayAttribute_Input =
{ 0xad1e2f3a, 0x4b5c, 0x6d7e, { 0x8f, 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f, 0x50 } };

// Vietnamese language ID (LANGID = 0x042A, LCID for Vietnamese - Vietnam)
static const LANGID TEXTSERVICE_LANGID = MAKELANGID(LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM);

// IME name
static const wchar_t TEXTSERVICE_DESC[]   = L"Nom Keyboard (Bàn phím Hán Nôm)";
static const wchar_t TEXTSERVICE_MODEL[]  = L"Apartment";

// Max candidates shown in window
static const int MAX_CANDIDATES_DISPLAY = 9;

// ---- Helper macro for COM reference counting ----
#define SafeRelease(p) { if (p) { (p)->Release(); (p) = nullptr; } }
