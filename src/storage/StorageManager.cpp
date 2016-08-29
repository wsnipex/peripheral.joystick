/*
 *      Copyright (C) 2015 Garrett Brown
 *      Copyright (C) 2015 Team XBMC
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

#include "StorageManager.h"
#include "JustABunchOfFiles.h"
#include "StorageUtils.h"
#include "buttonmapper/ButtonMapper.h"
#include "log/Log.h"
#include "storage/api/DatabaseJoystickAPI.h"
//#include "storage/retroarch/DatabaseRetroarch.h" // TODO
#include "storage/xml/DatabaseXml.h"
#include "utils/StringUtils.h"

#include "libKODI_peripheral.h"
#include "kodi_peripheral_types.h"
#include "kodi_peripheral_utils.hpp"

using namespace JOYSTICK;

// Resources folder for add-on and user data
#define USER_RESOURCES_FOLDER   "resources"
#define ADDON_RESOURCES_FOLDER  "resources"

// Subdirectory under resources folder for storing button maps
#define BUTTONMAP_FOLDER        "buttonmaps"

CStorageManager::CStorageManager(void) :
  m_peripheralLib(nullptr)
{
}

CStorageManager::~CStorageManager()
{
  Deinitialize();
}

CStorageManager& CStorageManager::Get(void)
{
  static CStorageManager _instance;
  return _instance;
}

bool CStorageManager::Initialize(ADDON::CHelper_libKODI_peripheral* peripheralLib,
                                 const PERIPHERAL_PROPERTIES& props)
{
  std::string strUserPath = props.user_path ? props.user_path : "";
  std::string strAddonPath = props.addon_path ? props.addon_path : "";

  if (peripheralLib == NULL || strUserPath.empty() || strAddonPath.empty())
    return false;

  m_peripheralLib = peripheralLib;

  m_buttonMapper.reset(new CButtonMapper(peripheralLib));

  if (!m_buttonMapper->Initialize(m_familyManager))
    return false;

  // Remove slash at end
  StringUtils::TrimRight(strUserPath, "\\/");
  StringUtils::TrimRight(strAddonPath, "\\/");

  strUserPath += "/" USER_RESOURCES_FOLDER;
  strAddonPath += "/" ADDON_RESOURCES_FOLDER;

  // Ensure resources path exists in user data
  CStorageUtils::EnsureDirectoryExists(strUserPath);

  std::string strUserButtonMapPath = strUserPath + "/" BUTTONMAP_FOLDER;
  std::string strAddonButtonMapPath = strAddonPath + "/" BUTTONMAP_FOLDER;

  // Ensure button map path exists in user data
  CStorageUtils::EnsureDirectoryExists(strUserButtonMapPath);

  m_databases.push_back(DatabasePtr(new CDatabaseXml(strUserButtonMapPath, true, m_buttonMapper->GetCallbacks())));
  //m_databases.push_back(DatabasePtr(new CDatabaseRetroArch(strUserButtonMapPath, true, &m_controllerMapper))); // TODO
  m_databases.push_back(DatabasePtr(new CDatabaseXml(strAddonButtonMapPath, false, m_buttonMapper->GetCallbacks())));
  //m_databases.push_back(DatabasePtr(new CDatabaseRetroArch(strAddonButtonMapPath, false))); // TODO

  m_databases.push_back(DatabasePtr(new CDatabaseJoystickAPI(m_buttonMapper->GetCallbacks())));

  for (auto& database : m_databases)
    m_buttonMapper->RegisterDatabase(database);

  m_familyManager.Initialize(strAddonPath);

  return true;
}

void CStorageManager::Deinitialize(void)
{
  m_familyManager.Deinitialize();
  m_databases.clear();
  m_buttonMapper.reset();
  m_peripheralLib = nullptr;
}

bool CStorageManager::GetFeatures(const ADDON::Joystick& joystick,
                                  const std::string& strControllerId,
                                  FeatureVector& features)
{
  if (m_buttonMapper)
    m_buttonMapper->GetFeatures(joystick, strControllerId, features);

  return !features.empty();
}

bool CStorageManager::MapFeatures(const ADDON::Joystick& joystick,
                                  const std::string& strControllerId,
                                  const FeatureVector& features)
{
  bool bModified = false;

  for (DatabaseVector::const_iterator it = m_databases.begin(); it != m_databases.end(); ++it)
    bModified |= (*it)->MapFeatures(joystick, strControllerId, features);

  return bModified;
}

bool CStorageManager::SaveButtonMap(const ADDON::Joystick& joystick)
{
  bool bModified = false;

  for (DatabaseVector::const_iterator it = m_databases.begin(); it != m_databases.end(); ++it)
    bModified |= (*it)->SaveButtonMap(joystick);

  return bModified;
}

bool CStorageManager::ResetButtonMap(const ADDON::Joystick& joystick, const std::string& strControllerId)
{
  bool bModified = false;

  for (DatabaseVector::const_iterator it = m_databases.begin(); it != m_databases.end(); ++it)
    bModified |= (*it)->ResetButtonMap(joystick, strControllerId);

  return bModified;
}

void CStorageManager::RefreshButtonMaps(const std::string& strDeviceName /* = "" */)
{
  // Request the frontend to refresh its button maps
  if (m_peripheralLib)
    m_peripheralLib->RefreshButtonMaps(strDeviceName);
}
