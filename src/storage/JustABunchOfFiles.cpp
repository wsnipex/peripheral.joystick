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

#include "JustABunchOfFiles.h"
#include "StorageDefinitions.h"
#include "StorageUtils.h"
#include "filesystem/DirectoryUtils.h"
#include "log/Log.h"
#include "utils/StringUtils.h"

#include <algorithm>

using namespace JOYSTICK;
using namespace P8PLATFORM;

#define FOLDER_DEPTH  1  // Recurse into max 1 subdirectories (provider)

// --- CResources --------------------------------------------------------------

CResources::~CResources(void)
{
  for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it)
    delete it->second;
}

DevicePtr CResources::GetDevice(const CDevice& deviceInfo)
{
  DevicePtr device;

  auto itDevice = m_devices.find(deviceInfo);
  if (itDevice != m_devices.end())
    device = itDevice->second;

  return device;
}

CButtonMap* CResources::GetResource(const CDevice& deviceInfo)
{
  CButtonMap* buttonMap = nullptr;

  auto itResource = m_resources.find(deviceInfo);
  if (itResource != m_resources.end())
    buttonMap = itResource->second;

  return buttonMap;
}

bool CResources::AddResource(CButtonMap* resource)
{
  if (resource != nullptr && resource->IsValid())
  {
    CButtonMap* oldResource = m_resources[*resource->Device()];
    delete oldResource;
    m_resources[*resource->Device()] = resource;
    m_devices[*resource->Device()] = resource->Device();
    return true;
  }
  return false;
}

void CResources::RemoveResource(const std::string& strPath)
{
  for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it)
  {
    if (it->second->Path() == strPath)
    {
      delete it->second;
      m_resources.erase(it);
      break;
    }
  }
}

// --- CJustABunchOfFiles ------------------------------------------------------

CJustABunchOfFiles::CJustABunchOfFiles(const std::string& strResourcePath,
                                       const std::string& strExtension,
                                       bool bReadWrite,
                                       IDatabaseCallbacks* callbacks) :
  IDatabase(callbacks),
  m_strResourcePath(strResourcePath),
  m_strExtension(strExtension),
  m_bReadWrite(bReadWrite)
{
  m_directoryCache.Initialize(this);

  if (m_bReadWrite)
    CStorageUtils::EnsureDirectoryExists(m_strResourcePath);
}

CJustABunchOfFiles::~CJustABunchOfFiles(void)
{
  m_directoryCache.Deinitialize();
}

const ButtonMap& CJustABunchOfFiles::GetButtonMap(const ADDON::Joystick& driverInfo)
{
  static ButtonMap empty;

  CLockObject lock(m_mutex);

  // Update index
  IndexDirectory(m_strResourcePath, FOLDER_DEPTH);

  CButtonMap* resource = m_resources.GetResource(driverInfo);

  if (resource)
    return resource->GetButtonMap();

  return empty;
}

bool CJustABunchOfFiles::MapFeatures(const ADDON::Joystick& driverInfo,
                                     const std::string& controllerId,
                                     const FeatureVector& features)
{
  if (!m_bReadWrite)
    return false;

  CLockObject lock(m_mutex);

  CButtonMap* resource = m_resources.GetResource(driverInfo);
  if (resource == nullptr)
  {
    // Resource doesn't exist yet, try to create it now
    std::string resourcePath;
    if (GetResourcePath(driverInfo, resourcePath))
    {
      DevicePtr device = std::make_shared<CDevice>(driverInfo);
      resource = CreateResource(resourcePath, device);
      if (!m_resources.AddResource(resource))
      {
        delete resource;
        resource = nullptr;
      }
    }
  }

  if (resource)
  {
    resource->MapFeatures(controllerId, features);
    return true;
  }

  return false;
}

bool CJustABunchOfFiles::SaveButtonMap(const ADDON::Joystick& driverInfo)
{
  if (!m_bReadWrite)
    return false;

  CDevice device(driverInfo);

  CLockObject lock(m_mutex);

  CButtonMap* resource = m_resources.GetResource(device);

  if (resource)
    return resource->SaveButtonMap();

  return false;
}

bool CJustABunchOfFiles::ResetButtonMap(const ADDON::Joystick& driverInfo, const std::string& controllerId)
{
  if (!m_bReadWrite)
    return false;

  CDevice device(driverInfo);

  CLockObject lock(m_mutex);

  CButtonMap* resource = m_resources.GetResource(device);

  if (resource)
    return resource->ResetButtonMap(controllerId);

  return false;
}

void CJustABunchOfFiles::IndexDirectory(const std::string& path, unsigned int folderDepth)
{
  // Enumerate the directory
  std::vector<ADDON::CVFSDirEntry> items;
  if (!m_directoryCache.GetDirectory(path, items))
    CDirectoryUtils::GetDirectory(path, m_strExtension + "|", items);

  // Recurse into subdirectories
  if (folderDepth > 0)
  {
    for (std::vector<ADDON::CVFSDirEntry>::const_iterator it = items.begin(); it != items.end(); ++it)
    {
      const ADDON::CVFSDirEntry& item = *it;
      if (item.IsFolder())
      {
        IndexDirectory(item.Path(), folderDepth - 1);
      }
    }
  }

  // Erase all folders and resources with different extensions
  items.erase(std::remove_if(items.begin(), items.end(),
    [this](const ADDON::CVFSDirEntry& item)
    {
      return !item.IsFolder() && !StringUtils::EndsWith(item.Path(), this->m_strExtension);
    }), items.end());

  m_directoryCache.UpdateDirectory(path, items);
}

void CJustABunchOfFiles::OnAdd(const ADDON::CVFSDirEntry& item)
{
  if (!item.IsFolder())
  {
    // TODO: Switch to unique_ptr or shared_ptr
    CButtonMap* resource = CreateResource(item.Path());

    // Load device info
    if (resource && resource->Refresh())
    {
      if (m_resources.AddResource(resource))
        m_callbacks->OnAdd(resource->Device(), resource->GetButtonMap());
      else
        delete resource;
    }
    else
      delete resource;
  }
}

void CJustABunchOfFiles::OnRemove(const ADDON::CVFSDirEntry& item)
{
  m_resources.RemoveResource(item.Path());
}

bool CJustABunchOfFiles::GetResourcePath(const ADDON::Joystick& deviceInfo, std::string& resourcePath) const
{
  // Calculate folder path
  std::string strFolder = m_strResourcePath + "/" + deviceInfo.Provider();

  // Calculate resource path
  resourcePath = strFolder + "/" + CStorageUtils::RootFileName(deviceInfo) + m_strExtension;

  // Ensure folder path exists
  return CStorageUtils::EnsureDirectoryExists(strFolder);
}
