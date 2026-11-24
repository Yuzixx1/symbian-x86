// qemu-port/genuine/hal/gen/hal_config.cpp
//
// Copyright (c) 1999-2026 Nokia Corporation and/or its subsidiary(-ies).// All rights reserved.
//
// GENERATED FILE - DO NOT EDIT
//

#include <kernel/hal_int.h>

GLREF_C TInt GetCPUSpeed(TInt, TInt, TBool, TAny*);	// ECPUSpeed
GLREF_C TInt GetSystemStartupReason(TInt, TInt, TBool, TAny*);	// ESystemStartupReason
GLREF_C TInt GetSystemException(TInt, TInt, TBool, TAny*);	// ESystemException
GLREF_C TInt GetRAMSize(TInt, TInt, TBool, TAny*);	// EMemoryRAM
GLREF_C TInt GetFreeRAM(TInt, TInt, TBool, TAny*);	// EMemoryRAMFree
GLREF_C TInt GetROMSize(TInt, TInt, TBool, TAny*);	// EMemoryROM
GLREF_C TInt GetPowerGoodState(TInt, TInt, TBool, TAny*);	// EPowerGood
GLREF_C TInt GetBatteryStatus(TInt, TInt, TBool, TAny*);	// EPowerBatteryStatus
GLREF_C TInt GetBackupPresent(TInt, TInt, TBool, TAny*);	// EPowerBackup
GLREF_C TInt GetBackupStatus(TInt, TInt, TBool, TAny*);	// EPowerBackupStatus
GLREF_C TInt GetPowerExternalState(TInt, TInt, TBool, TAny*);	// EPowerExternal
GLREF_C TInt ProcessKeyboardInfo(TInt, TInt, TBool, TAny*);	// EKeyboard
GLREF_C TInt ProcessKeyboardInfo(TInt, TInt, TBool, TAny*);	// EKeyboardDeviceKeys
GLREF_C TInt ProcessKeyboardInfo(TInt, TInt, TBool, TAny*);	// EKeyboardAppKeys
GLREF_C TInt GetKeyboardClickPresent(TInt, TInt, TBool, TAny*);	// EKeyboardClick
GLREF_C TInt ProcessKeyboardClickState(TInt, TInt, TBool, TAny*);	// EKeyboardClickState
GLREF_C TInt ProcessKeyboardClickVolume(TInt, TInt, TBool, TAny*);	// EKeyboardClickVolume
GLREF_C TInt GetKeyboardClickVolumeMax(TInt, TInt, TBool, TAny*);	// EKeyboardClickVolumeMax
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayXPixels
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayYPixels
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayXTwips
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayYTwips
GLREF_C TInt ProcessDisplayColors(TInt, TInt, TBool, TAny*);	// EDisplayColors
GLREF_C TInt ProcessDisplayState(TInt, TInt, TBool, TAny*);	// EDisplayState
GLREF_C TInt ProcessDisplayContrast(TInt, TInt, TBool, TAny*);	// EDisplayContrast
GLREF_C TInt ProcessDisplayMaxContrast(TInt, TInt, TBool, TAny*);	// EDisplayContrastMax
GLREF_C TInt GetBacklightPresent(TInt, TInt, TBool, TAny*);	// EBacklight
GLREF_C TInt ProcessBacklightState(TInt, TInt, TBool, TAny*);	// EBacklightState
GLREF_C TInt GetPenPresent(TInt, TInt, TBool, TAny*);	// EPen
GLREF_C TInt ProcessPenInfo(TInt, TInt, TBool, TAny*);	// EPenX
GLREF_C TInt ProcessPenInfo(TInt, TInt, TBool, TAny*);	// EPenY
GLREF_C TInt ProcessPenDisplayOnState(TInt, TInt, TBool, TAny*);	// EPenDisplayOn
GLREF_C TInt GetPenClickPresent(TInt, TInt, TBool, TAny*);	// EPenClick
GLREF_C TInt ProcessPenClickState(TInt, TInt, TBool, TAny*);	// EPenClickState
GLREF_C TInt ProcessPenClickVolume(TInt, TInt, TBool, TAny*);	// EPenClickVolume
GLREF_C TInt GetPenClickVolumeMax(TInt, TInt, TBool, TAny*);	// EPenClickVolumeMax
GLREF_C TInt GetMousePresent(TInt, TInt, TBool, TAny*);	// EMouse
GLREF_C TInt ProcessMouseInfo(TInt, TInt, TBool, TAny*);	// EMouseX
GLREF_C TInt ProcessMouseInfo(TInt, TInt, TBool, TAny*);	// EMouseY
GLREF_C TInt ProcessMouseState(TInt, TInt, TBool, TAny*);	// EMouseState
GLREF_C TInt ProcessMouseSpeed(TInt, TInt, TBool, TAny*);	// EMouseSpeed
GLREF_C TInt ProcessMouseAcceleration(TInt, TInt, TBool, TAny*);	// EMouseAcceleration
GLREF_C TInt ProcessMouseInfo(TInt, TInt, TBool, TAny*);	// EMouseButtons
GLREF_C TInt GetMouseButtonState(TInt, TInt, TBool, TAny*);	// EMouseButtonState
GLREF_C TInt ProcessCaseSwitchDisplayOnState(TInt, TInt, TBool, TAny*);	// ECaseSwitchDisplayOn
GLREF_C TInt ProcessCaseSwitchDisplayOffState(TInt, TInt, TBool, TAny*);	// ECaseSwitchDisplayOff
GLREF_C TInt GetLedCaps(TInt, TInt, TBool, TAny*);	// ELEDs
GLREF_C TInt ProcessLEDMask(TInt, TInt, TBool, TAny*);	// ELEDmask
GLREF_C TInt ProcessDisplayBrightness(TInt, TInt, TBool, TAny*);	// EDisplayBrightness
GLREF_C TInt ProcessDisplayMaxBrightness(TInt, TInt, TBool, TAny*);	// EDisplayBrightnessMax
GLREF_C TInt GetAccessoryPowerPresent(TInt, TInt, TBool, TAny*);	// EAccessoryPower
GLREF_C TInt ProcessKeyboardState(TInt, TInt, TBool, TAny*);	// EKeyboardState
GLREF_C TInt ProcessPenState(TInt, TInt, TBool, TAny*);	// EPenState
GLREF_C TInt ProcessDisplaySpecifiedModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayIsMono
GLREF_C TInt ProcessDisplaySpecifiedModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayIsPalettized
GLREF_C TInt ProcessDisplaySpecifiedModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayBitsPerPixel
GLREF_C TInt ProcessDisplayNumModes(TInt, TInt, TBool, TAny*);	// EDisplayNumModes
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayMemoryAddress
GLREF_C TInt ProcessDisplaySpecifiedModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayOffsetToFirstPixel
GLREF_C TInt ProcessDisplaySpecifiedModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayOffsetBetweenLines
GLREF_C TInt ProcessDisplayPaletteEntry(TInt, TInt, TBool, TAny*);	// EDisplayPaletteEntry
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayIsPixelOrderRGB
GLREF_C TInt ProcessDisplayCurrentModeInfo(TInt, TInt, TBool, TAny*);	// EDisplayIsPixelOrderLandscape
GLREF_C TInt ProcessDisplayMode(TInt, TInt, TBool, TAny*);	// EDisplayMode
GLREF_C TInt GetSwitches(TInt, TInt, TBool, TAny*);	// ESwitches
GLREF_C TInt ProcessDebugPort(TInt, TInt, TBool, TAny*);	// EDebugPort
GLREF_C TInt ProcessCustomRestart(TInt, TInt, TBool, TAny*);	// ECustomRestart
GLREF_C TInt ProcessCustomRestartReason(TInt, TInt, TBool, TAny*);	// ECustomRestartReason
GLREF_C TInt ProcessPersistStartupMode(TInt, TInt, TBool, TAny*);	// EPersistStartupModeKernel
GLREF_C TInt GetMaximumCustomRestartReasons(TInt, TInt, TBool, TAny*);	// EMaximumCustomRestartReasons
GLREF_C TInt GetMaximumRestartStartupModes(TInt, TInt, TBool, TAny*);	// EMaximumRestartStartupModes
GLREF_C TInt GetCPUProfilerInterrupt(TInt, TInt, TBool, TAny*);	// ECpuProfilingDefaultInterruptBase
GLREF_C TInt GetNumCpus(TInt, TInt, TBool, TAny*);	// ENumCpus
GLREF_C TInt DigitiserOrientation(TInt, TInt, TBool, TAny*);	// EDigitiserOrientation

const TUint8 HalInternal::Properties[]=
	{
	HAL::EValid,		// EManufacturer
	HAL::EValid,		// EManufacturerHardwareRev
	HAL::EValid,		// EManufacturerSoftwareRev
	HAL::EValid,		// EManufacturerSoftwareBuild
	HAL::EValid,		// EModel
	HAL::EValid,		// EMachineUid
	HAL::EValid,		// EDeviceFamily
	HAL::EValid,		// EDeviceFamilyRev
	HAL::EValid,		// ECPU
	HAL::EValid,		// ECPUArch
	HAL::EValid,		// ECPUABI
	HAL::EValid,		// ECPUSpeed
	HAL::EValid,		// ESystemStartupReason
	HAL::EValid,		// ESystemException
	HAL::EValid,		// ESystemTickPeriod
	HAL::EValid,		// EMemoryRAM
	HAL::EValid,		// EMemoryRAMFree
	HAL::EValid,		// EMemoryROM
	HAL::EValid,		// EMemoryPageSize
	HAL::EValid,		// EPowerGood
	HAL::EValid,		// EPowerBatteryStatus
	HAL::EValid,		// EPowerBackup
	HAL::EValid,		// EPowerBackupStatus
	HAL::EValid,		// EPowerExternal
	HAL::EValid,		// EKeyboard
	HAL::EValid,		// EKeyboardDeviceKeys
	HAL::EValid,		// EKeyboardAppKeys
	HAL::EValid,		// EKeyboardClick
	HAL::EValid|HAL::ESettable,		// EKeyboardClickState
	HAL::EValid|HAL::ESettable,		// EKeyboardClickVolume
	HAL::EValid,		// EKeyboardClickVolumeMax
	HAL::EValid,		// EDisplayXPixels
	HAL::EValid,		// EDisplayYPixels
	HAL::EValid,		// EDisplayXTwips
	HAL::EValid,		// EDisplayYTwips
	HAL::EValid,		// EDisplayColors
	HAL::EValid|HAL::ESettable,		// EDisplayState
	HAL::EValid|HAL::ESettable,		// EDisplayContrast
	HAL::EValid,		// EDisplayContrastMax
	HAL::EValid,		// EBacklight
	HAL::EValid|HAL::ESettable,		// EBacklightState
	HAL::EValid,		// EPen
	HAL::EValid,		// EPenX
	HAL::EValid,		// EPenY
	HAL::EValid|HAL::ESettable,		// EPenDisplayOn
	HAL::EValid,		// EPenClick
	HAL::EValid|HAL::ESettable,		// EPenClickState
	HAL::EValid|HAL::ESettable,		// EPenClickVolume
	HAL::EValid,		// EPenClickVolumeMax
	HAL::EValid,		// EMouse
	HAL::EValid,		// EMouseX
	HAL::EValid,		// EMouseY
	HAL::EValid|HAL::ESettable,		// EMouseState
	HAL::EValid|HAL::ESettable,		// EMouseSpeed
	HAL::EValid|HAL::ESettable,		// EMouseAcceleration
	HAL::EValid,		// EMouseButtons
	HAL::EValid,		// EMouseButtonState
	HAL::EValid,		// ECaseState
	HAL::EValid,		// ECaseSwitch
	HAL::EValid,		// ECaseSwitchDisplayOn
	HAL::EValid,		// ECaseSwitchDisplayOff
	HAL::EValid,		// ELEDs
	HAL::EValid|HAL::ESettable,		// ELEDmask
	HAL::EValid,		// EIntegratedPhone
	HAL::EValid|HAL::ESettable,		// EDisplayBrightness
	HAL::EValid,		// EDisplayBrightnessMax
	HAL::EValid,		// EKeyboardBacklightState
	HAL::EValid,		// EAccessoryPower
	HAL::EValid|HAL::ESettable,		// ELanguageIndex
	HAL::EValid|HAL::ESettable,		// EKeyboardIndex
	HAL::EValid,		// EMaxRAMDriveSize
	HAL::EValid|HAL::ESettable,		// EKeyboardState
	HAL::EValid|HAL::ESettable,		// ESystemDrive
	HAL::EValid|HAL::ESettable,		// EPenState
	HAL::EValid,		// EDisplayIsMono
	HAL::EValid,		// EDisplayIsPalettized
	HAL::EValid,		// EDisplayBitsPerPixel
	HAL::EValid,		// EDisplayNumModes
	HAL::EValid,		// EDisplayMemoryAddress
	HAL::EValid,		// EDisplayOffsetToFirstPixel
	HAL::EValid,		// EDisplayOffsetBetweenLines
	HAL::EValid|HAL::ESettable,		// EDisplayPaletteEntry
	HAL::EValid,		// EDisplayIsPixelOrderRGB
	HAL::EValid,		// EDisplayIsPixelOrderLandscape
	HAL::EValid|HAL::ESettable,		// EDisplayMode
	HAL::EValid,		// ESwitches
	HAL::EValid|HAL::ESettable,		// EDebugPort
	HAL::EValid,		// ELocaleLoaded
	HAL::EValid,		// EClipboardDrive
	HAL::EValid|HAL::ESettable,		// ECustomRestart
	HAL::EValid,		// ECustomRestartReason
	HAL::EValid,		// EDisplayNumberOfScreens
	HAL::EValid,		// ENanoTickPeriod
	HAL::EValid,		// EFastCounterFrequency
	HAL::EValid,		// EFastCounterCountsUp
	0,		// EPointer3D
	0,		// EPointer3DMaxProximity
	0,		// EPointer3DThetaSupported
	0,		// EPointer3DPhiSupported
	0,		// EPointer3DRotationSupported
	0,		// EPointer3DPressureSupported
	0,		// EHardwareFloatingPoint
	0,		// ETimeNonSecureOffset
	HAL::EValid|HAL::ESettable,		// EPersistStartupModeKernel
	HAL::EValid,		// EMaximumCustomRestartReasons
	HAL::EValid,		// EMaximumRestartStartupModes
	HAL::EValid|HAL::ESettable,		// ECustomResourceDrive
	0,		// EPointer3DProximityStep
	0,		// EPointerMaxPointers
	0,		// EPointerNumberOfPointers
	0,		// EPointer3DMaxPressure
	0,		// EPointer3DPressureStep
	0,		// EPointer3DEnterHighPressureThreshold
	0,		// EPointer3DExitHighPressureThreshold
	0,		// EPointer3DEnterCloseProximityThreshold
	0,		// EPointer3DExitCloseProximityThreshold
	0,		// EDisplayMemoryHandle
	0,		// ESerialNumber
	HAL::EValid,		// ECpuProfilingDefaultInterruptBase
	HAL::EValid,		// ENumCpus
	HAL::EValid|HAL::ESettable		// EDigitiserOrientation
	};

#if 0
const TInt HalInternal::Offset[]=
	{
	0,		// EManufacturer
	4,		// EManufacturerHardwareRev
	8,		// EManufacturerSoftwareRev
	12,		// EManufacturerSoftwareBuild
	16,		// EModel
	20,		// EMachineUid
	24,		// EDeviceFamily
	28,		// EDeviceFamilyRev
	32,		// ECPU
	36,		// ECPUArch
	40,		// ECPUABI
	-1,		// ECPUSpeed
	-1,		// ESystemStartupReason
	-1,		// ESystemException
	44,		// ESystemTickPeriod
	-1,		// EMemoryRAM
	-1,		// EMemoryRAMFree
	-1,		// EMemoryROM
	48,		// EMemoryPageSize
	-1,		// EPowerGood
	-1,		// EPowerBatteryStatus
	-1,		// EPowerBackup
	-1,		// EPowerBackupStatus
	-1,		// EPowerExternal
	-1,		// EKeyboard
	-1,		// EKeyboardDeviceKeys
	-1,		// EKeyboardAppKeys
	-1,		// EKeyboardClick
	-1,		// EKeyboardClickState
	-1,		// EKeyboardClickVolume
	-1,		// EKeyboardClickVolumeMax
	-1,		// EDisplayXPixels
	-1,		// EDisplayYPixels
	-1,		// EDisplayXTwips
	-1,		// EDisplayYTwips
	-1,		// EDisplayColors
	-1,		// EDisplayState
	-1,		// EDisplayContrast
	-1,		// EDisplayContrastMax
	-1,		// EBacklight
	-1,		// EBacklightState
	-1,		// EPen
	-1,		// EPenX
	-1,		// EPenY
	-1,		// EPenDisplayOn
	-1,		// EPenClick
	-1,		// EPenClickState
	-1,		// EPenClickVolume
	-1,		// EPenClickVolumeMax
	-1,		// EMouse
	-1,		// EMouseX
	-1,		// EMouseY
	-1,		// EMouseState
	-1,		// EMouseSpeed
	-1,		// EMouseAcceleration
	-1,		// EMouseButtons
	-1,		// EMouseButtonState
	52,		// ECaseState
	56,		// ECaseSwitch
	-1,		// ECaseSwitchDisplayOn
	-1,		// ECaseSwitchDisplayOff
	-1,		// ELEDs
	-1,		// ELEDmask
	60,		// EIntegratedPhone
	-1,		// EDisplayBrightness
	-1,		// EDisplayBrightnessMax
	64,		// EKeyboardBacklightState
	-1,		// EAccessoryPower
	68,		// ELanguageIndex
	72,		// EKeyboardIndex
	76,		// EMaxRAMDriveSize
	-1,		// EKeyboardState
	80,		// ESystemDrive
	-1,		// EPenState
	-1,		// EDisplayIsMono
	-1,		// EDisplayIsPalettized
	-1,		// EDisplayBitsPerPixel
	-1,		// EDisplayNumModes
	-1,		// EDisplayMemoryAddress
	-1,		// EDisplayOffsetToFirstPixel
	-1,		// EDisplayOffsetBetweenLines
	-1,		// EDisplayPaletteEntry
	-1,		// EDisplayIsPixelOrderRGB
	-1,		// EDisplayIsPixelOrderLandscape
	-1,		// EDisplayMode
	-1,		// ESwitches
	-1,		// EDebugPort
	84,		// ELocaleLoaded
	88,		// EClipboardDrive
	-1,		// ECustomRestart
	-1,		// ECustomRestartReason
	92,		// EDisplayNumberOfScreens
	96,		// ENanoTickPeriod
	100,		// EFastCounterFrequency
	104,		// EFastCounterCountsUp
	-1,		// EPointer3D
	-1,		// EPointer3DMaxProximity
	-1,		// EPointer3DThetaSupported
	-1,		// EPointer3DPhiSupported
	-1,		// EPointer3DRotationSupported
	-1,		// EPointer3DPressureSupported
	-1,		// EHardwareFloatingPoint
	-1,		// ETimeNonSecureOffset
	-1,		// EPersistStartupModeKernel
	-1,		// EMaximumCustomRestartReasons
	-1,		// EMaximumRestartStartupModes
	108,		// ECustomResourceDrive
	-1,		// EPointer3DProximityStep
	-1,		// EPointerMaxPointers
	-1,		// EPointerNumberOfPointers
	-1,		// EPointer3DMaxPressure
	-1,		// EPointer3DPressureStep
	-1,		// EPointer3DEnterHighPressureThreshold
	-1,		// EPointer3DExitHighPressureThreshold
	-1,		// EPointer3DEnterCloseProximityThreshold
	-1,		// EPointer3DExitCloseProximityThreshold
	-1,		// EDisplayMemoryHandle
	-1,		// ESerialNumber
	-1,		// ECpuProfilingDefaultInterruptBase
	-1,		// ENumCpus
	-1		// EDigitiserOrientation
	};

#endif
const TInt HalInternal::HalDataSize=112;

const THalImplementation HalInternal::Implementation[]=
	{
	NULL,		// EManufacturer
	NULL,		// EManufacturerHardwareRev
	NULL,		// EManufacturerSoftwareRev
	NULL,		// EManufacturerSoftwareBuild
	NULL,		// EModel
	NULL,		// EMachineUid
	NULL,		// EDeviceFamily
	NULL,		// EDeviceFamilyRev
	NULL,		// ECPU
	NULL,		// ECPUArch
	NULL,		// ECPUABI
	GetCPUSpeed,		// ECPUSpeed
	GetSystemStartupReason,		// ESystemStartupReason
	GetSystemException,		// ESystemException
	NULL,		// ESystemTickPeriod
	GetRAMSize,		// EMemoryRAM
	GetFreeRAM,		// EMemoryRAMFree
	GetROMSize,		// EMemoryROM
	NULL,		// EMemoryPageSize
	GetPowerGoodState,		// EPowerGood
	GetBatteryStatus,		// EPowerBatteryStatus
	GetBackupPresent,		// EPowerBackup
	GetBackupStatus,		// EPowerBackupStatus
	GetPowerExternalState,		// EPowerExternal
	ProcessKeyboardInfo,		// EKeyboard
	ProcessKeyboardInfo,		// EKeyboardDeviceKeys
	ProcessKeyboardInfo,		// EKeyboardAppKeys
	GetKeyboardClickPresent,		// EKeyboardClick
	ProcessKeyboardClickState,		// EKeyboardClickState
	ProcessKeyboardClickVolume,		// EKeyboardClickVolume
	GetKeyboardClickVolumeMax,		// EKeyboardClickVolumeMax
	ProcessDisplayCurrentModeInfo,		// EDisplayXPixels
	ProcessDisplayCurrentModeInfo,		// EDisplayYPixels
	ProcessDisplayCurrentModeInfo,		// EDisplayXTwips
	ProcessDisplayCurrentModeInfo,		// EDisplayYTwips
	ProcessDisplayColors,		// EDisplayColors
	ProcessDisplayState,		// EDisplayState
	ProcessDisplayContrast,		// EDisplayContrast
	ProcessDisplayMaxContrast,		// EDisplayContrastMax
	GetBacklightPresent,		// EBacklight
	ProcessBacklightState,		// EBacklightState
	GetPenPresent,		// EPen
	ProcessPenInfo,		// EPenX
	ProcessPenInfo,		// EPenY
	ProcessPenDisplayOnState,		// EPenDisplayOn
	GetPenClickPresent,		// EPenClick
	ProcessPenClickState,		// EPenClickState
	ProcessPenClickVolume,		// EPenClickVolume
	GetPenClickVolumeMax,		// EPenClickVolumeMax
	GetMousePresent,		// EMouse
	ProcessMouseInfo,		// EMouseX
	ProcessMouseInfo,		// EMouseY
	ProcessMouseState,		// EMouseState
	ProcessMouseSpeed,		// EMouseSpeed
	ProcessMouseAcceleration,		// EMouseAcceleration
	ProcessMouseInfo,		// EMouseButtons
	GetMouseButtonState,		// EMouseButtonState
	NULL,		// ECaseState
	NULL,		// ECaseSwitch
	ProcessCaseSwitchDisplayOnState,		// ECaseSwitchDisplayOn
	ProcessCaseSwitchDisplayOffState,		// ECaseSwitchDisplayOff
	GetLedCaps,		// ELEDs
	ProcessLEDMask,		// ELEDmask
	NULL,		// EIntegratedPhone
	ProcessDisplayBrightness,		// EDisplayBrightness
	ProcessDisplayMaxBrightness,		// EDisplayBrightnessMax
	NULL,		// EKeyboardBacklightState
	GetAccessoryPowerPresent,		// EAccessoryPower
	NULL,		// ELanguageIndex
	NULL,		// EKeyboardIndex
	NULL,		// EMaxRAMDriveSize
	ProcessKeyboardState,		// EKeyboardState
	NULL,		// ESystemDrive
	ProcessPenState,		// EPenState
	ProcessDisplaySpecifiedModeInfo,		// EDisplayIsMono
	ProcessDisplaySpecifiedModeInfo,		// EDisplayIsPalettized
	ProcessDisplaySpecifiedModeInfo,		// EDisplayBitsPerPixel
	ProcessDisplayNumModes,		// EDisplayNumModes
	ProcessDisplayCurrentModeInfo,		// EDisplayMemoryAddress
	ProcessDisplaySpecifiedModeInfo,		// EDisplayOffsetToFirstPixel
	ProcessDisplaySpecifiedModeInfo,		// EDisplayOffsetBetweenLines
	ProcessDisplayPaletteEntry,		// EDisplayPaletteEntry
	ProcessDisplayCurrentModeInfo,		// EDisplayIsPixelOrderRGB
	ProcessDisplayCurrentModeInfo,		// EDisplayIsPixelOrderLandscape
	ProcessDisplayMode,		// EDisplayMode
	GetSwitches,		// ESwitches
	ProcessDebugPort,		// EDebugPort
	NULL,		// ELocaleLoaded
	NULL,		// EClipboardDrive
	ProcessCustomRestart,		// ECustomRestart
	ProcessCustomRestartReason,		// ECustomRestartReason
	NULL,		// EDisplayNumberOfScreens
	NULL,		// ENanoTickPeriod
	NULL,		// EFastCounterFrequency
	NULL,		// EFastCounterCountsUp
	NULL,		// EPointer3D
	NULL,		// EPointer3DMaxProximity
	NULL,		// EPointer3DThetaSupported
	NULL,		// EPointer3DPhiSupported
	NULL,		// EPointer3DRotationSupported
	NULL,		// EPointer3DPressureSupported
	NULL,		// EHardwareFloatingPoint
	NULL,		// ETimeNonSecureOffset
	ProcessPersistStartupMode,		// EPersistStartupModeKernel
	GetMaximumCustomRestartReasons,		// EMaximumCustomRestartReasons
	GetMaximumRestartStartupModes,		// EMaximumRestartStartupModes
	NULL,		// ECustomResourceDrive
	NULL,		// EPointer3DProximityStep
	NULL,		// EPointerMaxPointers
	NULL,		// EPointerNumberOfPointers
	NULL,		// EPointer3DMaxPressure
	NULL,		// EPointer3DPressureStep
	NULL,		// EPointer3DEnterHighPressureThreshold
	NULL,		// EPointer3DExitHighPressureThreshold
	NULL,		// EPointer3DEnterCloseProximityThreshold
	NULL,		// EPointer3DExitCloseProximityThreshold
	NULL,		// EDisplayMemoryHandle
	NULL,		// ESerialNumber
	GetCPUProfilerInterrupt,		// ECpuProfilingDefaultInterruptBase
	GetNumCpus,		// ENumCpus
	DigitiserOrientation		// EDigitiserOrientation
	};

