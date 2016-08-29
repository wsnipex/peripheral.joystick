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

#include "storage/IDatabase.h"

namespace JOYSTICK
{
  class CDatabaseJoystickAPI : public IDatabase
  {
  public:
    CDatabaseJoystickAPI(IDatabaseCallbacks* callbacks) : IDatabase(callbacks) { }

    virtual ~CDatabaseJoystickAPI(void) { }

    // implementation of IDatabase
    virtual const ButtonMap& GetButtonMap(const ADDON::Joystick& driverInfo) override;
    virtual bool MapFeatures(const ADDON::Joystick& driverInfo, const std::string& controllerId, const FeatureVector& features) override;
    virtual bool ResetButtonMap(const ADDON::Joystick& driverInfo, const std::string& controllerId) override;
    virtual bool SaveButtonMap(const ADDON::Joystick& driverInfo) override;
  };
}
