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
#include "ButtonMapUtils.h"
#include "storage/Device.h"
#include "utils/CommonMacros.h"

#include "kodi_peripheral_utils.hpp"

#include <algorithm>

using namespace JOYSTICK;

CControllerTransformer::CControllerTransformer(CJoystickFamilyManager& familyManager) :
  m_familyManager(familyManager)
{
}

void CControllerTransformer::OnAdd(const DevicePtr& driverInfo, const ButtonMap& buttonMap)
{
  // Santity check
  if (m_observedDevices.size() > 200)
    return;

  // Skip devices we've already encountered.
  if (m_observedDevices.find(driverInfo) != m_observedDevices.end())
    return;

  m_observedDevices.insert(driverInfo);

  for (auto itTo = buttonMap.begin(); itTo != buttonMap.end(); ++itTo)
  {
    // Only allow controller map items where "from" compares before "to"
    for (auto itFrom = buttonMap.begin(); itFrom->first < itTo->first; ++itFrom)
    {
      AddControllerMap(itFrom->first, itFrom->second, itTo->first, itTo->second);
    }
  }
}

DevicePtr CControllerTransformer::CreateDevice(const CDevice& deviceInfo)
{
  DevicePtr result = std::make_shared<CDevice>(deviceInfo);

  for (const auto& device : m_observedDevices)
  {
    if (*device == deviceInfo)
      result->Configuration() = device->Configuration();
  }

  return result;
}

void CControllerTransformer::AddControllerMap(const std::string& controllerFrom, const FeatureVector& featuresFrom,
                                              const std::string& controllerTo, const FeatureVector& featuresTo)
{
  const bool bSwap = (controllerFrom >= controllerTo);

  ControllerTranslation key = { bSwap ? controllerTo : controllerFrom,
                                bSwap ? controllerFrom : controllerTo };

  FeatureMaps& featureMaps = m_controllerMap[key];

  FeatureMap featureMap = CreateFeatureMap(bSwap ? featuresTo : featuresFrom,
                                           bSwap ? featuresFrom : featuresTo);

  auto it = featureMaps.find(featureMap);

  if (it == featureMaps.end())
  {
    featureMaps.insert(std::make_pair(std::move(featureMap), 1));
  }
  else
  {
    ++it->second;
  }
}

FeatureMap CControllerTransformer::CreateFeatureMap(const FeatureVector& featuresFrom, const FeatureVector& featuresTo)
{
  FeatureMap featureMap;

  for (const ADDON::JoystickFeature& featureFrom : featuresFrom)
  {
    for (JOYSTICK_FEATURE_PRIMITIVE primitiveIndex : ButtonMapUtils::GetPrimitives(featureFrom.Type()))
    {
      const ADDON::DriverPrimitive& targetPrimitive = featureFrom.Primitive(primitiveIndex);

      if (targetPrimitive.Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN)
        continue;

      JOYSTICK_FEATURE_PRIMITIVE toPrimitiveIndex;

      auto itFeatureTo = std::find_if(featuresTo.begin(), featuresTo.end(),
        [&targetPrimitive, &toPrimitiveIndex](const ADDON::JoystickFeature& featureTo)
        {
          for (JOYSTICK_FEATURE_PRIMITIVE toIndex : ButtonMapUtils::GetPrimitives(featureTo.Type()))
          {
            if (targetPrimitive == featureTo.Primitive(toIndex))
            {
              toPrimitiveIndex = toIndex;
              return true;
            }
          }
          return false;
        });

      if (itFeatureTo != featuresTo.end())
      {
        FeaturePrimitive fromPrimitive = { featureFrom, primitiveIndex };
        FeaturePrimitive toPrimitive = { *itFeatureTo, toPrimitiveIndex };

        featureMap.insert(std::make_pair(std::move(fromPrimitive), std::move(toPrimitive)));
      }
    }
  }

  return featureMap;
}

void CControllerTransformer::TransformFeatures(const ADDON::Joystick& driverInfo,
                                               const std::string& fromController,
                                               const std::string& toController,
                                               const FeatureVector& features,
                                               FeatureVector& transformedFeatures)
{
  const bool bSwap = (fromController >= toController);

  ControllerTranslation key = { bSwap ? toController : fromController,
                                bSwap ? fromController : toController };

  const FeatureMaps& featureMaps = m_controllerMap[key];

  const FeatureMap& featureMap = GetFeatureMap(featureMaps);

  for (const ADDON::JoystickFeature& sourceFeature : features)
  {
    for (JOYSTICK_FEATURE_PRIMITIVE primitiveIndex : ButtonMapUtils::GetPrimitives(sourceFeature.Type()))
    {
      const ADDON::DriverPrimitive& sourcePrimitive = sourceFeature.Primitive(primitiveIndex);

      if (sourcePrimitive.Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN)
        continue;

      ADDON::JoystickFeature targetFeature;
      JOYSTICK_FEATURE_PRIMITIVE targetPrimitive;

      if (TranslatePrimitive(sourceFeature, primitiveIndex, targetFeature, targetPrimitive, featureMap, bSwap))
        SetPrimitive(transformedFeatures, targetFeature, targetPrimitive, sourcePrimitive);
    }
  }
}

const FeatureMap& CControllerTransformer::GetFeatureMap(const FeatureMaps& featureMaps)
{
  static const FeatureMap empty;

  unsigned int maxCount = 0;
  const FeatureMap* bestFeatureMap = nullptr;

  for (const auto& featureMap : featureMaps)
  {
    const FeatureMap& features = featureMap.first;
    unsigned int count = featureMap.second;

    if (count > maxCount)
    {
      maxCount = count;
      bestFeatureMap = &features;
    }
  }

  if (bestFeatureMap != nullptr)
    return *bestFeatureMap;

  return empty;
}

bool CControllerTransformer::TranslatePrimitive(const ADDON::JoystickFeature& sourceFeature,
                                                JOYSTICK_FEATURE_PRIMITIVE sourcePrimitive,
                                                ADDON::JoystickFeature& targetFeature,
                                                JOYSTICK_FEATURE_PRIMITIVE& targetPrimitive,
                                                const FeatureMap& featureMap,
                                                bool bSwap)
{
  auto itFeatureMap = std::find_if(featureMap.begin(), featureMap.end(),
    [&sourceFeature, sourcePrimitive, bSwap](const std::pair<FeaturePrimitive, FeaturePrimitive>& featureEntry)
    {
      if (bSwap)
      {
        return sourceFeature.Name() == featureEntry.second.feature.Name() &&
               sourcePrimitive == featureEntry.second.primitive;
      }
      else
      {
        return sourceFeature.Name() == featureEntry.first.feature.Name() &&
               sourcePrimitive == featureEntry.first.primitive;
      }
    });

  if (itFeatureMap != featureMap.end())
  {
    targetFeature = bSwap ? itFeatureMap->first.feature :
                            itFeatureMap->second.feature;
    targetPrimitive = bSwap ? itFeatureMap->first.primitive :
                              itFeatureMap->second.primitive;
    return true;
  }

  return false;
}

void CControllerTransformer::SetPrimitive(FeatureVector& features,
                                          const ADDON::JoystickFeature& feature,
                                          JOYSTICK_FEATURE_PRIMITIVE index,
                                          const ADDON::DriverPrimitive& primitive)
{
  auto itFeature = std::find_if(features.begin(), features.end(),
    [&feature](const ADDON::JoystickFeature& targetFeature)
    {
      return feature.Name() == targetFeature.Name();
    });

  if (itFeature == features.end())
  {
    ADDON::JoystickFeature newFeature(feature.Name(), feature.Type());
    newFeature.SetPrimitive(index, primitive);
    features.emplace_back(std::move(newFeature));
  }
  else
  {
    itFeature->SetPrimitive(index, primitive);
  }
}
