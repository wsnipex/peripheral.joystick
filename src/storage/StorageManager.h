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
#include "buttonmapper/JoystickFamily.h"

#include <memory>
#include <string>

struct PERIPHERAL_PROPERTIES;

namespace ADDON
{
  class CHelper_libKODI_peripheral;
  class Joystick;
}

namespace JOYSTICK
{
  class CButtonMapper;
  class CDevice;
  class IDatabase;

  class CStorageManager
  {
  private:
    CStorageManager(void);

  public:
    static CStorageManager& Get(void);

    ~CStorageManager(void);

    /*!
     * \brief Initialize storage manager
     *
     * \param peripheralLib The peripheral API helper library
     * \param props used in add-on creation (TODO: Change to two strings)
     *
     * \return true if the storage manager has been initialized and can be safely used
     */
    bool Initialize(ADDON::CHelper_libKODI_peripheral* peripheralLib,
                    const PERIPHERAL_PROPERTIES& props);

    /*!
     * \brief Deinitialize storage manager
     */
    void Deinitialize(void);

    /*!
     * \brief Get the map of features to driver primitives from a storage backend
     *
     * \param joystick      The device's joystick properties; unknown values may be left at their default
     * \param controller_id The controller profile being requested, e.g. game.controller.default
     * \param features      The array of features and their driver primitives
     *
     * \return true if results were loaded from a storage backend
     */
    bool GetFeatures(const ADDON::Joystick& joystick,
                     const std::string& strDeviceId,
                     FeatureVector& features);

    /*!
     * \brief Update button maps
     *
     * \param joystick      The device's joystick properties; unknown values may be left at their default
     * \param controller_id The game controller profile being updated
     * \param features      The array of features and their driver primitives
     *
     * \return true if features were mapped in a storage backend
     */
    bool MapFeatures(const ADDON::Joystick& joystick,
                     const std::string& strDeviceId,
                     const FeatureVector& features);

    /*!
     * \brief Save the button map for the specified device
     *
     * \param deviceName The name of the device to reset
     *
     * \return true if the underlying storage was modified, false otherwise
     */
    bool SaveButtonMap(const ADDON::Joystick& joystick);

    /*!
     * \brief Reset the button map for the specified device and controller profile
     *
     * \param deviceName The name of the device to reset
     * \param controllerId The controller ID to reset
     *
     * \return true if the underlying storage was modified, false otherwise
     */
    bool ResetButtonMap(const ADDON::Joystick& joystick, const std::string& strControllerId);

    /*!
     * \brief Notify the frontend that button maps have changed
     *
     * \param[optional] deviceName The name of the device to refresh, or empty for all devices
     * \param[optional] controllerId The controller ID to refresh, or empty for all controllers
     */
    void RefreshButtonMaps(const std::string& strDeviceName = "");

  private:
    ADDON::CHelper_libKODI_peripheral* m_peripheralLib;

    DatabaseVector                 m_databases;
    std::unique_ptr<CButtonMapper> m_buttonMapper;
    CJoystickFamilyManager         m_familyManager;
  };
}
