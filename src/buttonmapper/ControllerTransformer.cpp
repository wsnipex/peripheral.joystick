/*
 *      Copyright (C) 2016 Garrett Brown
 *      Copyright (C) 2016 Team Kodi
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
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "ControllerTransformer.h"
#include "storage/Device.h"
#include "utils/CommonMacros.h"

#include "kodi_peripheral_utils.hpp"

#include <algorithm>
#include <array>

using namespace JOYSTICK;

CControllerTransformer::CControllerTransformer(CJoystickFamilyManager& familyManager) :
  m_familyManager(familyManager)
{
}

CControllerTransformer::~CControllerTransformer()
{
}

void CControllerTransformer::OnAdd(const DevicePtr& driverInfo, const ButtonMap& buttonMap)
{
  // Skip devices we've already encountered.
  if (m_observedDevices.find(driverInfo) == m_observedDevices.end())
    m_observedDevices.insert(driverInfo);
  else
    return;

  for (auto itTo = buttonMap.begin(); itTo != buttonMap.end(); ++itTo)
  {
    // Only allow controller map items where "from" compares before "to"
    for (auto itFrom = buttonMap.begin(); itFrom->first < itTo->first; ++itFrom)
    {
      AddControllerMap(itFrom->first, itFrom->second, itTo->first, itTo->second);
    }
  }
}

bool CControllerTransformer::AddControllerMap(const std::string& controllerFrom, const FeatureVector& featuresFrom,
                                              const std::string& controllerTo, const FeatureVector& featuresTo)
{
  bool bChanged = false;

  ASSERT(controllerFrom < controllerTo);

  ControllerMapItem needle = { controllerFrom, controllerTo };

  for (auto itFromFeature = featuresFrom.begin(); itFromFeature != featuresFrom.end(); ++itFromFeature)
  {
    const ADDON::JoystickFeature& fromFeature = *itFromFeature;

    auto itToFeature = std::find_if(featuresTo.begin(), featuresTo.end(),
      [&fromFeature](const ADDON::JoystickFeature& feature)
      {
        if (fromFeature.Type() == feature.Type())
        {
          return fromFeature.Primitives() == feature.Primitives();
          switch (feature.Type())
          {
          case JOYSTICK_FEATURE_TYPE_SCALAR:
          case JOYSTICK_FEATURE_TYPE_MOTOR:
          {
            return fromFeature.Primitive(JOYSTICK_SCALAR_PRIMITIVE) == feature.Primitive(JOYSTICK_SCALAR_PRIMITIVE);
          }
          case JOYSTICK_FEATURE_TYPE_ANALOG_STICK:
          {
            return fromFeature.Primitive(JOYSTICK_ANALOG_STICK_UP)    == feature.Primitive(JOYSTICK_ANALOG_STICK_UP) &&
                   fromFeature.Primitive(JOYSTICK_ANALOG_STICK_DOWN)  == feature.Primitive(JOYSTICK_ANALOG_STICK_DOWN) &&
                   fromFeature.Primitive(JOYSTICK_ANALOG_STICK_RIGHT) == feature.Primitive(JOYSTICK_ANALOG_STICK_RIGHT) &&
                   fromFeature.Primitive(JOYSTICK_ANALOG_STICK_LEFT)  == feature.Primitive(JOYSTICK_ANALOG_STICK_LEFT);
          }
          case JOYSTICK_FEATURE_TYPE_ACCELEROMETER:
          {
            return fromFeature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_X) == feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_X) &&
                   fromFeature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Y) == feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Y) &&
                   fromFeature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Z) == feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Z);
          }
          default:
            break;
          }
        }
        return false;
      });

    if (itToFeature != featuresTo.end())
    {
      FeatureMapItem featureMapItem = { fromFeature.Name(), itToFeature->Name() };
      m_controllerModel.AddFeatureMapping(needle, std::move(featureMapItem));
      bChanged = true;
    }
  }

  return bChanged;
}

void CControllerTransformer::TransformFeatures(const ADDON::Joystick& driverInfo,
                                               const std::string& fromController,
                                               const std::string& toController,
                                               const FeatureVector& features,
                                               FeatureVector& transformedFeatures)
{
  bool bSwap = (fromController >= toController);

  ControllerMapItem needle = { bSwap ? toController : fromController,
                               bSwap ? fromController : toController };

  std::array<CControllerModel*, 1> models = { &m_controllerModel };

  for (CControllerModel* model : models)
  {
    const FeatureOccurrences& normalizedFeatures = model->GetNormalizedFeatures(needle);

    for (auto itMap = normalizedFeatures.begin(); itMap != normalizedFeatures.end(); ++itMap)
    {
      const std::string& fromFeature = bSwap ? itMap->first.toFeature : itMap->first.fromFeature;
      const std::string& toFeature = bSwap ? itMap->first.fromFeature : itMap->first.toFeature;

      auto itFrom = std::find_if(features.begin(), features.end(),
        [fromFeature](const ADDON::JoystickFeature& feature)
        {
          return feature.Name() == fromFeature;
        });

      if (itFrom != features.end())
      {
        ADDON::JoystickFeature transformedFeature(*itFrom);
        transformedFeature.SetName(toFeature);
        transformedFeatures.emplace_back(std::move(transformedFeature));
      }
    }

    if (!transformedFeatures.empty())
      break;
  }
}
