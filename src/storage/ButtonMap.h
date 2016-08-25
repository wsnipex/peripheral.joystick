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

#include "StorageTypes.h"
#include "buttonmapper/ButtonMapTypes.h"

#include <string>

namespace JOYSTICK
{
  class CButtonMap
  {
  public:
    CButtonMap(const std::string& strResourcePath);
    CButtonMap(const std::string& strResourcePath, const DevicePtr& device);

    virtual ~CButtonMap(void) { }

    const std::string& Path(void) const { return m_strResourcePath; }

    const DevicePtr& Device(void) const { return m_device; }

    bool IsValid(void) const;

    const ButtonMap& GetButtonMap();

    void GetControllerMap(ControllerMap& controllerMap);

    bool MapFeatures(const std::string& controllerId, const FeatureVector& features);

    bool ResetButtonMap(const std::string& controllerId);

    bool Refresh(void);

  protected:
    virtual bool Load(void) = 0;
    virtual bool Save(void) const = 0;

    static void Sanitize(const std::string& controllerId, FeatureVector& features);

    const std::string m_strResourcePath;
    DevicePtr         m_device;
    ButtonMap         m_buttonMap;

  private:
    int64_t m_timestamp;
  };
}
