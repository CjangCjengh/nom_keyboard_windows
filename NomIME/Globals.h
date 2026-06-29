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
// NOTE: must be a unique, freshly-generated v4 UUID. The previous hand-crafted
// value {7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D} collided with a built-in Windows
// WinRT CLSID (PrintSupportEnterpriseManagementUIEventArgsFactory in
// Windows.Graphics.Printing.Workflow.dll) which is present on Win11, so TSF's
// CoCreateInstance for the text service resolved to the wrong class and the IME
// appeared in the list but did nothing when selected.
// {FF82830D-8CF7-4DBF-8C03-6380E2BB655F}
static const CLSID CLSID_NomTextService =
{ 0xff82830d, 0x8cf7, 0x4dbf, { 0x8c, 0x03, 0x63, 0x80, 0xe2, 0xbb, 0x65, 0x5f } };

// ---- Profile GUID (language profile for Vietnamese) ----
// {47411A8D-264C-4C5E-BF2F-14A75591E378}
static const GUID GUID_NomProfile =
{ 0x47411a8d, 0x264c, 0x4c5e, { 0xbf, 0x2f, 0x14, 0xa7, 0x55, 0x91, 0xe3, 0x78 } };

// ---- Compartment GUID for open/close ----
// {8054BB9A-22CB-4595-83F2-58314EAA48B7}
static const GUID GUID_NomCompartment =
{ 0x8054bb9a, 0x22cb, 0x4595, { 0x83, 0xf2, 0x58, 0x31, 0x4e, 0xaa, 0x48, 0xb7 } };

// ---- Display attribute GUIDs ----
// {6CECA4BD-A8A8-4EF8-BD19-E84375FF0FA5}
static const GUID GUID_NomDisplayAttribute_Input =
{ 0x6ceca4bd, 0xa8a8, 0x4ef8, { 0xbd, 0x19, 0xe8, 0x43, 0x75, 0xff, 0x0f, 0xa5 } };

// Vietnamese language ID (LANGID = 0x042A, LCID for Vietnamese - Vietnam)
static const LANGID TEXTSERVICE_LANGID = MAKELANGID(LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM);

// IME display names (chosen by system language at registration time)
static const wchar_t TEXTSERVICE_DESC_VI[] = L"B\x00e0n ph\x00edm H\x00e1n N\x00f4m";   // "Bàn phím Hán Nôm"
static const wchar_t TEXTSERVICE_DESC_EN[] = L"Nom Keyboard";
static const wchar_t TEXTSERVICE_DESC[]    = L"B\x00e0n ph\x00edm H\x00e1n N\x00f4m";    // fallback
static const wchar_t TEXTSERVICE_MODEL[]   = L"Apartment";

// Max candidates shown in window
static const int MAX_CANDIDATES_DISPLAY = 9;

// ---- Helper macro for COM reference counting ----
#define SafeRelease(p) { if (p) { (p)->Release(); (p) = nullptr; } }
