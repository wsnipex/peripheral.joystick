/*
 *      Copyright (C) 2014-2015 Garrett Brown
 *      Copyright (C) 2014-2015 Team XBMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#define PERIPHERAL_ADDON_JOYSTICKS

#include "api/Joystick.h"
#include "api/JoystickManager.h"
#include "api/PeripheralScanner.h"
#include "filesystem/Filesystem.h"
#include "log/Log.h"
#include "log/LogAddon.h"
#include "settings/Settings.h"
#include "storage/StorageManager.h"
#include "utils/CommonIncludes.h" // for libXBMC_addon.h
#include "utils/CommonMacros.h"

#include "libKODI_peripheral.h"
#include "xbmc_addon_dll.h"
#include "kodi_peripheral_dll.h"
#include "kodi_peripheral_utils.hpp"

#include <algorithm>
#include <vector>

using namespace JOYSTICK;

extern "C"
{

ADDON::CHelper_libXBMC_addon*      FRONTEND;
ADDON::CHelper_libKODI_peripheral* PERIPHERAL;
CPeripheralScanner*                SCANNER;

ADDON_STATUS ADDON_Create(void* callbacks, void* props)
{
  PERIPHERAL_PROPERTIES* peripheralProps = static_cast<PERIPHERAL_PROPERTIES*>(props);

  try
  {
    if (!callbacks || !peripheralProps)
      throw ADDON_STATUS_UNKNOWN;

    FRONTEND = new ADDON::CHelper_libXBMC_addon;
    if (!FRONTEND || !FRONTEND->RegisterMe(callbacks))
      throw ADDON_STATUS_PERMANENT_FAILURE;

    PERIPHERAL = new ADDON::CHelper_libKODI_peripheral;
    if (!PERIPHERAL || !PERIPHERAL->RegisterMe(callbacks))
      throw ADDON_STATUS_PERMANENT_FAILURE;
  }
  catch (const ADDON_STATUS& status)
  {
    SAFE_DELETE(PERIPHERAL);
    SAFE_DELETE(FRONTEND);
    return status;
  }

  CLog::Get().SetPipe(new CLogAddon(FRONTEND));

  if (!CFilesystem::Initialize(FRONTEND))
    return ADDON_STATUS_PERMANENT_FAILURE;

  SCANNER = new CPeripheralScanner(PERIPHERAL);
  if (!CJoystickManager::Get().Initialize(SCANNER))
    return ADDON_STATUS_PERMANENT_FAILURE;

  if (!CStorageManager::Get().Initialize(PERIPHERAL, *peripheralProps))
    return ADDON_STATUS_PERMANENT_FAILURE;

  return ADDON_GetStatus();
}

void ADDON_Stop()
{
}

void ADDON_Destroy()
{
  CStorageManager::Get().Deinitialize();
  CJoystickManager::Get().Deinitialize();
  CFilesystem::Deinitialize();

  CLog::Get().SetType(SYS_LOG_TYPE_CONSOLE);

  SAFE_DELETE(PERIPHERAL);
  SAFE_DELETE(FRONTEND);
  SAFE_DELETE(SCANNER);
}

ADDON_STATUS ADDON_GetStatus()
{
  if (!FRONTEND || !PERIPHERAL)
    return ADDON_STATUS_UNKNOWN;

  if (!CSettings::Get().IsInitialized())
    return ADDON_STATUS_NEED_SETTINGS;

  return ADDON_STATUS_OK;
}

bool ADDON_HasSettings()
{
  return false;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char* settingName, const void* settingValue)
{
  if (settingName && settingValue)
    CSettings::Get().SetSetting(settingName, settingValue);

  return ADDON_STATUS_OK;
}

void ADDON_FreeSettings()
{
}

void ADDON_Announce(const char* flag, const char* sender, const char* message, const void* data)
{
}

const char* GetPeripheralAPIVersion(void)
{
  return PERIPHERAL_API_VERSION;
}

const char* GetMinimumPeripheralAPIVersion(void)
{
  return PERIPHERAL_MIN_API_VERSION;
}

PERIPHERAL_ERROR GetAddonCapabilities(PERIPHERAL_CAPABILITIES* pCapabilities)
{
  if (!pCapabilities)
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  pCapabilities->provides_joysticks = true;
  pCapabilities->provides_buttonmaps = true;

  return PERIPHERAL_NO_ERROR;
}

PERIPHERAL_ERROR PerformDeviceScan(unsigned int* peripheral_count, PERIPHERAL_INFO** scan_results)
{
  if (!peripheral_count || !scan_results)
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  JoystickVector joysticks;
  if (!CJoystickManager::Get().PerformJoystickScan(joysticks))
    return PERIPHERAL_ERROR_FAILED;

  // Upcast array pointers
  std::vector<ADDON::Peripheral*> peripherals;
  for (JoystickVector::const_iterator it = joysticks.begin(); it != joysticks.end(); ++it)
    peripherals.push_back(it->get());

  *peripheral_count = peripherals.size();
  ADDON::Peripherals::ToStructs(peripherals, scan_results);

  return PERIPHERAL_NO_ERROR;
}

void FreeScanResults(unsigned int peripheral_count, PERIPHERAL_INFO* scan_results)
{
  ADDON::Peripherals::FreeStructs(peripheral_count, scan_results);
}

PERIPHERAL_ERROR GetEvents(unsigned int* event_count, PERIPHERAL_EVENT** events)
{
  if (!event_count || !events)
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  PERIPHERAL_ERROR result = PERIPHERAL_ERROR_FAILED;

  std::vector<ADDON::PeripheralEvent> peripheralEvents;
  if (CJoystickManager::Get().GetEvents(peripheralEvents))
  {
    *event_count = peripheralEvents.size();
    ADDON::PeripheralEvents::ToStructs(peripheralEvents, events);
    result = PERIPHERAL_NO_ERROR;
  }

  CJoystickManager::Get().ProcessEvents();

  return result;
}

void FreeEvents(unsigned int event_count, PERIPHERAL_EVENT* events)
{
  ADDON::PeripheralEvents::FreeStructs(event_count, events);
}

bool SendEvent(const PERIPHERAL_EVENT* event)
{
  bool bHandled = false;

  if (event != nullptr)
    bHandled = CJoystickManager::Get().SendEvent(*event);

  return bHandled;
}

PERIPHERAL_ERROR GetJoystickInfo(unsigned int index, JOYSTICK_INFO* info)
{
  if (!info)
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  JoystickPtr joystick = CJoystickManager::Get().GetJoystick(index);
  if (!joystick)
    return PERIPHERAL_ERROR_NOT_CONNECTED;

  // Need to be explicit because we're using typedef struct { ... }T instead of struct T{ ... }
  joystick->ADDON::Joystick::ToStruct(*info);

  return PERIPHERAL_NO_ERROR;
}

void FreeJoystickInfo(JOYSTICK_INFO* info)
{
  if (!info)
    return;

  ADDON::Joystick::FreeStruct(*info);
}

PERIPHERAL_ERROR GetFeatures(const JOYSTICK_INFO* joystick, const char* controller_id,
                             unsigned int* feature_count, JOYSTICK_FEATURE** features)
{
  if (!joystick || !controller_id || !feature_count || !features)
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  FeatureVector featureVector;
  if (CStorageManager::Get().GetFeatures(ADDON::Joystick(*joystick), controller_id,  featureVector))
  {
    *feature_count = featureVector.size();
    ADDON::JoystickFeatures::ToStructs(featureVector, features);
    return PERIPHERAL_NO_ERROR;
  }

  return PERIPHERAL_ERROR_FAILED;
}

void FreeFeatures(unsigned int feature_count, JOYSTICK_FEATURE* features)
{
  ADDON::JoystickFeatures::FreeStructs(feature_count, features);
}

PERIPHERAL_ERROR MapFeatures(const JOYSTICK_INFO* joystick, const char* controller_id,
                             unsigned int feature_count, JOYSTICK_FEATURE* features)
{
  if (!joystick || !controller_id || (feature_count > 0 && !features))
    return PERIPHERAL_ERROR_INVALID_PARAMETERS;

  FeatureVector featureVector(features, features + feature_count);
  bool bSuccess = CStorageManager::Get().MapFeatures(ADDON::Joystick(*joystick), controller_id, featureVector);

  return bSuccess ? PERIPHERAL_NO_ERROR : PERIPHERAL_ERROR_FAILED;
}

void SaveButtonMap(const JOYSTICK_INFO* joystick)
{
  if (joystick == nullptr)
    return;

  ADDON::Joystick addonJoystick(*joystick);

  if (CStorageManager::Get().SaveButtonMap(addonJoystick));
    PERIPHERAL->RefreshButtonMaps(addonJoystick.Name());
}

void ResetButtonMap(const JOYSTICK_INFO* joystick, const char* controller_id)
{
  if (!joystick || !controller_id)
    return;

  ADDON::Joystick addonJoystick(*joystick);

  if (CStorageManager::Get().ResetButtonMap(addonJoystick, controller_id))
    PERIPHERAL->RefreshButtonMaps(addonJoystick.Name());
}

void PowerOffJoystick(unsigned int index)
{
  JoystickPtr joystick = CJoystickManager::Get().GetJoystick(index);
  if (!joystick || !joystick->SupportsPowerOff())
    return;

  joystick->PowerOff();
}

} // extern "C"
