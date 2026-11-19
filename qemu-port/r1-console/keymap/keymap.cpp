// Copyright (c) 1996-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the License "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
// Contributors:
// Sybian port contributors - bounded US-layout R1 data tables.
//
// Adapted table/export format and modifier rules from upstream
// bsptemplate/asspandvariant/template_variant/specific/keymap.cpp.
// NOT a private translator. Compiled in EKDATA v3; never executed. See ../README.md.
#include <k32keys.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// One singleton Symbian scan-code block per row. Physical Set-1 conversion
// belongs to the input adapter, never to this genuine EKDATA interface.
// Deliberately no Fn/power/case/menu/device/media/keyboard-extend actions.
#define FIXED_KEYS(K) \
    K(EStdKeyBackspace, EKeyBackspace) \
    K(EStdKeyTab, EKeyTab) \
    K(EStdKeyEnter, EKeyEnter) \
    K(EStdKeyEscape, EKeyEscape) \
    K(EStdKeySpace, ' ') \
    K(EStdKeyHome, EKeyHome) \
    K(EStdKeyEnd, EKeyEnd) \
    K(EStdKeyPageUp, EKeyPageUp) \
    K(EStdKeyPageDown, EKeyPageDown) \
    K(EStdKeyInsert, EKeyInsert) \
    K(EStdKeyDelete, EKeyDelete) \
    K(EStdKeyLeftArrow, EKeyLeftArrow) \
    K(EStdKeyRightArrow, EKeyRightArrow) \
    K(EStdKeyUpArrow, EKeyUpArrow) \
    K(EStdKeyDownArrow, EKeyDownArrow) \
    K(EStdKeyLeftShift, EKeyLeftShift) \
    K(EStdKeyRightShift, EKeyRightShift) \
    K(EStdKeyLeftAlt, EKeyLeftAlt) \
    K(EStdKeyRightAlt, EKeyRightAlt) \
    K(EStdKeyLeftCtrl, EKeyLeftCtrl) \
    K(EStdKeyRightCtrl, EKeyRightCtrl) \
    K(EStdKeyCapsLock, EKeyCapsLock)

// Explicit ordinary US punctuation and digit mapping (unshifted, shifted).
#define SYMBOL_KEYS(K) \
    K('0', '0', ')') \
    K('1', '1', '!') \
    K('2', '2', '@') \
    K('3', '3', '#') \
    K('4', '4', '$') \
    K('5', '5', '%') \
    K('6', '6', '^') \
    K('7', '7', '&') \
    K('8', '8', '*') \
    K('9', '9', '(') \
    K(EStdKeyXXX, '`', '~') \
    K(EStdKeyComma, ',', '<') \
    K(EStdKeyFullStop, '.', '>') \
    K(EStdKeyForwardSlash, '/', '?') \
    K(EStdKeyBackSlash, '\\', '|') \
    K(EStdKeySemiColon, ';', ':') \
    K(EStdKeySingleQuote, '\'', '"') \
    K(EStdKeySquareBracketLeft, '[', '{') \
    K(EStdKeySquareBracketRight, ']', '}') \
    K(EStdKeyMinus, '-', '_') \
    K(EStdKeyEquals, '=', '+')

#define FIXED_BLOCK(s,k) {s,s},
#define SYMBOL_BLOCK(s,b,u) {s,s},
static const SScanCodeBlock fixedBlocks[] = { FIXED_KEYS(FIXED_BLOCK) };
static const SScanCodeBlock symbolBlocks[] = { SYMBOL_KEYS(SYMBOL_BLOCK) };
#define FIXED_VALUE(s,k) k,
#define BASE_VALUE(s,b,u) b,
#define SHIFT_VALUE(s,b,u) u,
static const TUint16 fixedCodes[] = { FIXED_KEYS(FIXED_VALUE) };
static const TUint16 symbolCodes[] = { SYMBOL_KEYS(BASE_VALUE) };
static const TUint16 shiftedSymbols[] = { SYMBOL_KEYS(SHIFT_VALUE) };
static const SScanCodeBlock letterBlocks[] = { {'A', 'Z'} };
static const TUint16 lowerLetters[] = {
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z'
    };
static const TUint16 upperLetters[] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
    };
static const TUint16 ctrlLetters[] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26
    };

#define SUBTABLE(n,c,b) static const SConvSubTable n = { c, { COUNT(b), b } }
SUBTABLE(fixed, fixedCodes, fixedBlocks);
SUBTABLE(symbols, symbolCodes, symbolBlocks);
SUBTABLE(shifted, shiftedSymbols, symbolBlocks);
SUBTABLE(lower, lowerLetters, letterBlocks);
SUBTABLE(upper, upperLetters, letterBlocks);
SUBTABLE(control, ctrlLetters, letterBlocks);
static const SConvSubTable* const ctrlTables[] = { &control };
static const SConvSubTable* const upperTables[] = { &upper };
static const SConvSubTable* const lowerTables[] = { &lower };
static const SConvSubTable* const shiftTables[] = { &shifted };
static const SConvSubTable* const baseTables[] = { &fixed, &symbols, &lower };
// First matching scan in first matching node wins. Caps XOR Shift for letters;
// Caps never changes punctuation. Ctrl letters take precedence over case.
static const SConvTableNode nodes[] = {
    { { EModifierCtrl, EModifierCtrl }, COUNT(ctrlTables), ctrlTables },
    { { EModifierShift|EModifierCapsLock, EModifierShift }, COUNT(upperTables), upperTables },
    { { EModifierShift|EModifierCapsLock, EModifierCapsLock }, COUNT(upperTables), upperTables },
    { { EModifierShift|EModifierCapsLock, EModifierShift|EModifierCapsLock }, COUNT(lowerTables), lowerTables },
    { { EModifierShift, EModifierShift }, COUNT(shiftTables), shiftTables },
    { { 0, 0 }, COUNT(baseTables), baseTables }
    };
static const SConvTable conversions = { COUNT(nodes), nodes };

// Genuine translator recomputes aggregate modifiers when both side bits clear.
// On release clear ONLY the side bit so the other held side remains effective.
#define MODIFIER(k,side,aggregate) \
    { {EModifierKeyUp,0}, {k,EMatchKey}, {EStateUnchanged,ETurnOnModifier,side|aggregate} }, \
    { {EModifierKeyUp,EModifierKeyUp}, {k,EMatchKey}, {EStateUnchanged,ETurnOffModifier,side} }
static const SFuncTableEntry modifiers[] = {
    MODIFIER(EKeyLeftShift, EModifierLeftShift, EModifierShift),
    MODIFIER(EKeyRightShift, EModifierRightShift, EModifierShift),
    MODIFIER(EKeyLeftCtrl, EModifierLeftCtrl, EModifierCtrl),
    MODIFIER(EKeyRightCtrl, EModifierRightCtrl, EModifierCtrl),
    MODIFIER(EKeyLeftAlt, EModifierLeftAlt, EModifierAlt),
    MODIFIER(EKeyRightAlt, EModifierRightAlt, EModifierAlt),
    { {EModifierKeyUp,0}, {EKeyCapsLock,EMatchKey},
      {EStateUnchanged,EToggleModifier,EModifierCapsLock} }
    };
// Catch-all MUST be last: upstream getDefault indexes without a not-found guard.
static const SFuncTableEntry defaults[] = {
    { {EModifierKeyUp,EModifierKeyUp}, {EKeyNull,EAnyKey}, {EStateUnchanged,EDoNothing,0} },
    { {0,0}, {EKeyNull,EAnyModifierKey}, {EStateUnchanged,EDoNothing,0} },
    { {0,0}, {EKeyNull,EMatchKey}, {EStateUnchanged,EDoNothing,0} },
    { {0,0}, {EKeyNull,EAnyKey}, {EStateNormal,EPassKeyThru,0} }
    };
// No dead-key, Unicode-digit-entry or special state transitions in this profile.
static const SFuncTable states[13] = {
    {0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
    {0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL}
    };
static const SFuncTables functions = {
    {COUNT(defaults), defaults}, {COUNT(modifiers), modifiers}, COUNT(states), states
    };
static const TUint16 nonRepeat[] = {
    EKeyLeftShift,EKeyRightShift,EKeyLeftCtrl,EKeyRightCtrl,
    EKeyLeftAlt,EKeyRightAlt,EKeyCapsLock
    };

EXPORT_C void KeyDataSettings(TRadix& aRadix, TCtrlDigitsTermination& aTermination,
                             TInt& aDefaultCount, TInt& aMaximumCount)
    {
    aRadix = EDecimal;
    aTermination = ETerminationByCtrlUp;
    aDefaultCount = 3;
    aMaximumCount = 10; // No table action enters ctrl-digit mode.
    }

EXPORT_C void KeyDataFuncTable(SFuncTables& aTables)
    {
    aTables = functions;
    }

EXPORT_C void KeyDataConvTable(SConvTable& aTable, TUint& aFirst, TUint& aLast,
                              SScanCodeBlockList& aKeypad, SKeyCodeList& aNonRepeat)
    {
    aTable = conversions;
    aFirst = EStdKeyBackspace;
    aLast = EStdKeyEquals;
    aKeypad.numBlocks = 0;
    aKeypad.pblocks = NULL;
    aNonRepeat.numKeyCodes = COUNT(nonRepeat);
    aNonRepeat.pkeyCodes = nonRepeat;
    }
