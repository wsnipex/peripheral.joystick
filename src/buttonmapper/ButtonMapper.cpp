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

#include "ButtonMapper.h"
#include "storage/IDatabase.h"

#include "kodi_peripheral_utils.hpp"

#include <algorithm>
#include <iterator>

using namespace JOYSTICK;

void CButtonMapper::RegisterDatabase(const DatabasePtr& database)
{
  if (std::find(m_databases.begin(), m_databases.end(), database) == m_databases.end())
    m_databases.push_back(database);
}

void CButtonMapper::UnregisterDatabase(const DatabasePtr& database)
{
  m_databases.erase(std::remove(m_databases.begin(), m_databases.end(), database), m_databases.end());
}

bool CButtonMapper::GetFeatures(const ADDON::Joystick& joystick,
                                const std::string& strControllerId,
                                FeatureVector& features)
{
  // Accumulate available button maps for this device
  ButtonMap accumulatedMap = GetButtonMap(joystick);

  GetFeatures(std::move(accumulatedMap), strControllerId, features);

  return !features.empty();
}

ButtonMap CButtonMapper::GetButtonMap(const ADDON::Joystick& joystick) const
{
  ButtonMap accumulatedMap;

  for (DatabaseVector::const_iterator it = m_databases.begin(); it != m_databases.end(); ++it)
  {
    const ButtonMap& buttonMap = (*it)->GetButtonMap(joystick);
    MergeButtonMap(accumulatedMap, buttonMap);
  }

  return accumulatedMap;
}

void CButtonMapper::MergeButtonMap(ButtonMap& knownMap, const ButtonMap& newFeatures)
{
  for (auto it = newFeatures.begin(); it != newFeatures.end(); ++it)
  {
    const std::string& controllerId = it->first;
    const FeatureVector& features = it->second;

    MergeFeatures(knownMap[controllerId], features);
  }
}

void CButtonMapper::MergeFeatures(FeatureVector& features, const FeatureVector& newFeatures)
{
  for (const ADDON::JoystickFeature& newFeature : newFeatures)
  {
    const bool bFound = std::find_if(features.begin(), features.end(),
      [newFeature](const ADDON::JoystickFeature& feature)
      {
        return feature.Name() == newFeature.Name();
      }) != features.end();

    if (!bFound)
      features.push_back(newFeature);
  }
}

bool CButtonMapper::GetFeatures(ButtonMap&& buttonMap, const std::string& controllerId, FeatureVector& features)
{
  // Try to get a button map for the specified controller profile
  auto itController = buttonMap.find(controllerId);
  if (itController != buttonMap.end())
    features.swap(itController->second);

  // Try to derive a button map from relations between controller profiles
  if (features.empty())
    DeriveFeatures(controllerId, buttonMap, features);

  return !features.empty();
}

void CButtonMapper::DeriveFeatures(const std::string& toController, const ButtonMap& buttonMap, FeatureVector& transformedFeatures)
{
  // Obtain an iterator to the controller profile with the highest count of features defined
  unsigned int maxFeatures = 0;
  auto maxFeaturesIt = buttonMap.end();

  for (auto it = buttonMap.begin(); it != buttonMap.end(); ++it)
  {
    const unsigned int featureCount = it->second.size();
    if (featureCount > maxFeatures)
    {
      maxFeatures = featureCount;
      maxFeaturesIt = it;
    }
  }

  if (maxFeaturesIt != buttonMap.end())
  {
    // Transform the controller profile with the most features to the specified controller
    const std::string& fromController = maxFeaturesIt->first;
    const FeatureVector& features = maxFeaturesIt->second;

    m_controllerMapper.TransformFeatures(fromController, toController, features, transformedFeatures);
  }
}
