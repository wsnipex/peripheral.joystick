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
#pragma once

#include "ButtonMap.h"
#include "Device.h"
#include "IDatabase.h"
#include "filesystem/DirectoryCache.h"

#include "p8-platform/threads/mutex.h"

#include <map>
#include <string>

namespace JOYSTICK
{
  /*!
   * \brief Container class for device records and button maps
   */
  class CResources
  {
  public:
    CResources(void) { }
    ~CResources(void);

    DevicePtr GetDevice(const CDevice& deviceInfo);

    CButtonMap* GetResource(const CDevice& deviceInfo);
    bool AddResource(CButtonMap* resource);
    void RemoveResource(const std::string& strPath);

  private:
    typedef std::map<CDevice, DevicePtr>   DeviceMap;
    typedef std::map<CDevice, CButtonMap*> ResourceMap;

    DeviceMap   m_devices;
    ResourceMap m_resources;
  };

  class CJustABunchOfFiles : public IDatabase,
                             public IDirectoryCacheCallback
  {
  public:
    CJustABunchOfFiles(const std::string& strResourcePath,
                       const std::string& strExtension,
                       bool bReadWrite,
                       IDatabaseCallbacks* callbacks);

    virtual ~CJustABunchOfFiles(void);

    // implementation of IDatabase
    virtual const ButtonMap& GetButtonMap(const ADDON::Joystick& driverInfo) override;
    virtual bool MapFeatures(const ADDON::Joystick& driverInfo,
                             const std::string& controllerId,
                             const FeatureVector& features) override;
    virtual bool SaveButtonMap(const ADDON::Joystick& driverInfo) override;
    virtual bool ResetButtonMap(const ADDON::Joystick& driverInfo,
                                const std::string& controllerId) override;

    // implementation of IDirectoryCacheCallback
    virtual void OnAdd(const ADDON::CVFSDirEntry& item) override;
    virtual void OnRemove(const ADDON::CVFSDirEntry& item) override;

  protected:
    // Interface for child class to provide
    virtual CButtonMap* CreateResource(const std::string& resourcePath) = 0;
    virtual CButtonMap* CreateResource(const std::string& resourcePath, const DevicePtr& driverInfo) = 0;

  private:
    /*!
     * \brief Recursively index a path, enumerating the folder and updating
     *        the directory cache
     */
    void IndexDirectory(const std::string& path, unsigned int folderDepth);

    /*!
     * \brief Calculate and create a path for a device record
     *
     * The path is created if it doesn't exist.
     *
     * \return true if the path exists or was created
     */
    bool GetResourcePath(const ADDON::Joystick& deviceInfo, std::string& resourcePath) const;

    const std::string m_strResourcePath;
    const std::string m_strExtension;
    const bool        m_bReadWrite;
    CDirectoryCache   m_directoryCache;
    CResources        m_resources;
    P8PLATFORM::CMutex  m_mutex;
  };
}
