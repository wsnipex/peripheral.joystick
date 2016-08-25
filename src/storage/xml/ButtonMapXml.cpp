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

#include "ButtonMapXml.h"
#include "ButtonMapDefinitions.h"
#include "DeviceXml.h"
#include "api/AnomalousTrigger.h"
#include "api/Joystick.h"
#include "api/JoystickManager.h"
#include "buttonmapper/ButtonMapTranslator.h"
#include "storage/Device.h"
#include "log/Log.h"

#include "tinyxml.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

using namespace JOYSTICK;

CButtonMapXml::CButtonMapXml(const std::string& strResourcePath) :
  CButtonMap(strResourcePath)
{
}

CButtonMapXml::CButtonMapXml(const std::string& strResourcePath, const DevicePtr& device) :
  CButtonMap(strResourcePath, device)
{
}

bool CButtonMapXml::Load(void)
{
  TiXmlDocument xmlFile;
  if (!xmlFile.LoadFile(m_strResourcePath))
  {
    esyslog("Error opening %s: %s", m_strResourcePath.c_str(), xmlFile.ErrorDesc());
    return false;
  }

  TiXmlElement* pRootElement = xmlFile.RootElement();
  if (!pRootElement || pRootElement->NoChildren() || pRootElement->ValueStr() != BUTTONMAP_XML_ROOT)
  {
    esyslog("Can't find root <%s> tag", BUTTONMAP_XML_ROOT);
    return false;
  }

  const TiXmlElement* pDevice = pRootElement->FirstChildElement(BUTTONMAP_XML_ELEM_DEVICE);

  if (!pDevice)
  {
    esyslog("Can't find <%s> tag", BUTTONMAP_XML_ELEM_DEVICE);
    return false;
  }

  // Don't overwrite valid device
  if (!m_device->IsValid())
  {
    if (!CDeviceXml::Deserialize(pDevice, *m_device))
      return false;
  }

  const TiXmlElement* pController = pDevice->FirstChildElement(BUTTONMAP_XML_ELEM_CONTROLLER);

  if (!pController)
  {
    esyslog("Device \"%s\": can't find <%s> tag", m_device->Name().c_str(), BUTTONMAP_XML_ELEM_CONTROLLER);
    return false;
  }

  // For logging purposes
  unsigned int totalFeatureCount = 0;

  while (pController)
  {
    const char* id = pController->Attribute(BUTTONMAP_XML_ATTR_CONTROLLER_ID);
    if (!id)
    {
      esyslog("Device \"%s\": <%s> tag has no attribute \"%s\"", m_device->Name().c_str(),
              BUTTONMAP_XML_ELEM_CONTROLLER, BUTTONMAP_XML_ATTR_CONTROLLER_ID);
      return false;
    }

    FeatureVector features;
    if (!Deserialize(pController, features))
      return false;

    if (features.empty())
    {
      esyslog("Device \"%s\" has no features for controller %s", m_device->Name().c_str(), id);
    }
    else
    {
      totalFeatureCount += features.size();
      m_buttonMap[id] = std::move(features);
    }

    pController = pController->NextSiblingElement(BUTTONMAP_XML_ELEM_CONTROLLER);
  }

  dsyslog("Loaded device \"%s\" with %u controller profiles and %u total features", m_device->Name().c_str(), m_buttonMap.size(), totalFeatureCount);

  return true;
}

bool CButtonMapXml::Save(void) const
{
  TiXmlDocument xmlFile;

  TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "", "");
  xmlFile.LinkEndChild(decl);

  TiXmlElement rootElement(BUTTONMAP_XML_ROOT);
  TiXmlNode* root = xmlFile.InsertEndChild(rootElement);
  if (root == NULL)
    return false;

  TiXmlElement* pElem = root->ToElement();
  if (!pElem)
    return false;

  TiXmlElement deviceElement(BUTTONMAP_XML_ELEM_DEVICE);
  TiXmlNode* deviceNode = pElem->InsertEndChild(deviceElement);
  if (deviceNode == NULL)
    return false;

  TiXmlElement* deviceElem = deviceNode->ToElement();
  if (deviceElem == NULL)
    return false;

  CDeviceXml::Serialize(*m_device, deviceElem);

  if (!SerializeTriggers(deviceElem))
    return false;

  if (!SerializeButtonMaps(deviceElem))
    return false;

  return xmlFile.SaveFile(m_strResourcePath);
}

bool CButtonMapXml::SerializeTriggers(TiXmlElement* pElement) const
{
  std::map<unsigned int, CAnomalousTrigger*> triggers;

  // Get triggers
  JoystickVector joysticks = CJoystickManager::Get().GetJoysticks(*m_device);
  for (const auto& joystick : joysticks)
  {
    std::vector<CAnomalousTrigger*> triggerVec = joystick->GetAnomalousTriggers();
    for (CAnomalousTrigger* trigger : triggerVec)
      triggers[trigger->AxisIndex()] = trigger;
  }

  // Serialize triggers
  if (!triggers.empty())
  {
    TiXmlElement configurationElement(BUTTONMAP_XML_ELEM_CONFIGURATION);
    TiXmlNode* configurationNode = pElement->InsertEndChild(configurationElement);
    if (configurationNode == nullptr)
      return false;

    TiXmlElement* configurationElem = configurationNode->ToElement();
    if (configurationElem == nullptr)
      return false;

    for (auto itTrigger = triggers.begin(); itTrigger != triggers.end(); ++itTrigger)
    {
      if (!SerializeTrigger(configurationElem, itTrigger->second))
        return false;
    }
  }

  return true;
}

bool CButtonMapXml::SerializeTrigger(TiXmlElement* pElement, const CAnomalousTrigger* trigger)
{
  TiXmlElement axisElement(BUTTONMAP_XML_ELEM_AXIS);
  TiXmlNode* axisNode = pElement->InsertEndChild(axisElement);
  if (axisNode == nullptr)
    return false;

  TiXmlElement* axisElem = axisNode->ToElement();
  if (axisElem == nullptr)
    return false;

  axisElem->SetAttribute(BUTTONMAP_XML_ATTR_AXIS_INDEX, trigger->AxisIndex());
  axisElem->SetAttribute(BUTTONMAP_XML_ATTR_AXIS_CENTER, trigger->Center());
  axisElem->SetAttribute(BUTTONMAP_XML_ATTR_AXIS_RANGE, trigger->Range());

  return true;
}

bool CButtonMapXml::SerializeButtonMaps(TiXmlElement* pElement) const
{
  for (ButtonMap::const_iterator it = m_buttonMap.begin(); it != m_buttonMap.end(); ++it)
  {
    const ControllerID& controllerId = it->first;
    const FeatureVector& features = it->second;

    if (features.empty())
      continue;

    TiXmlElement profileElement(BUTTONMAP_XML_ELEM_CONTROLLER);
    TiXmlNode* profileNode = pElement->InsertEndChild(profileElement);
    if (profileNode == NULL)
      continue;

    TiXmlElement* profileElem = profileNode->ToElement();
    if (profileElem == NULL)
      continue;

    profileElem->SetAttribute(BUTTONMAP_XML_ATTR_CONTROLLER_ID, controllerId);

    Serialize(features, profileElem);
  }
  return true;
}

bool CButtonMapXml::Serialize(const FeatureVector& features, TiXmlElement* pElement)
{
  if (pElement == NULL)
    return false;

  for (FeatureVector::const_iterator it = features.begin(); it != features.end(); ++it)
  {
    const ADDON::JoystickFeature& feature = *it;

    if (!IsValid(feature))
      continue;

    TiXmlElement featureElement(BUTTONMAP_XML_ELEM_FEATURE);
    TiXmlNode* featureNode = pElement->InsertEndChild(featureElement);
    if (featureNode == NULL)
      return false;

    TiXmlElement* featureElem = featureNode->ToElement();
    if (featureElem == NULL)
      return false;

    featureElem->SetAttribute(BUTTONMAP_XML_ATTR_FEATURE_NAME, feature.Name());

    switch (feature.Type())
    {
      case JOYSTICK_FEATURE_TYPE_SCALAR:
      {
        SerializePrimitive(featureElem, feature.Primitive(JOYSTICK_SCALAR_PRIMITIVE));

        break;
      }
      case JOYSTICK_FEATURE_TYPE_ANALOG_STICK:
      {
        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ANALOG_STICK_UP), BUTTONMAP_XML_ELEM_UP))
          return false;

        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ANALOG_STICK_DOWN), BUTTONMAP_XML_ELEM_DOWN))
          return false;

        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ANALOG_STICK_RIGHT), BUTTONMAP_XML_ELEM_RIGHT))
          return false;

        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ANALOG_STICK_LEFT), BUTTONMAP_XML_ELEM_LEFT))
          return false;

        break;
      }
      case JOYSTICK_FEATURE_TYPE_ACCELEROMETER:
      {
        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_X), BUTTONMAP_XML_ELEM_POSITIVE_X))
          return false;

        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Y), BUTTONMAP_XML_ELEM_POSITIVE_Y))
          return false;

        if (!SerializePrimitiveTag(featureElem, feature.Primitive(JOYSTICK_ACCELEROMETER_POSITIVE_Z), BUTTONMAP_XML_ELEM_POSITIVE_Z))
          return false;

        break;
      }
      case JOYSTICK_FEATURE_TYPE_MOTOR:
      {
        SerializePrimitive(featureElem, feature.Primitive(JOYSTICK_MOTOR_PRIMITIVE));

        break;
      }
      default:
        break;
    }
  }

  return true;
}

bool CButtonMapXml::IsValid(const ADDON::JoystickFeature& feature)
{
  auto itValid = std::find_if(feature.Primitives().begin(), feature.Primitives().end(),
    [](const ADDON::DriverPrimitive& primitive)
    {
      return primitive.Type() != JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN;
    });

  return itValid != feature.Primitives().end();
}

bool CButtonMapXml::SerializePrimitiveTag(TiXmlElement* pElement, const ADDON::DriverPrimitive& primitive, const char* tagName)
{
  if (primitive.Type() != JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN)
  {
    if (pElement == NULL)
      return false;

    TiXmlElement primitiveElement(tagName);
    TiXmlNode* primitiveNode = pElement->InsertEndChild(primitiveElement);
    if (primitiveNode == NULL)
      return false;

    TiXmlElement* primitiveElem = primitiveNode->ToElement();
    if (primitiveElem == NULL)
      return false;

    SerializePrimitive(primitiveElem, primitive);
  }

  return true;
}

void CButtonMapXml::SerializePrimitive(TiXmlElement* pElement, const ADDON::DriverPrimitive& primitive)
{
  std::string strPrimitive = ButtonMapTranslator::ToString(primitive);
  if (!strPrimitive.empty())
  {
    switch (primitive.Type())
    {
      case JOYSTICK_DRIVER_PRIMITIVE_TYPE_BUTTON:
      {
        pElement->SetAttribute(BUTTONMAP_XML_ATTR_FEATURE_BUTTON, strPrimitive);
        break;
      }
      case JOYSTICK_DRIVER_PRIMITIVE_TYPE_HAT_DIRECTION:
      {
        pElement->SetAttribute(BUTTONMAP_XML_ATTR_FEATURE_HAT, strPrimitive);
        break;
      }
      case JOYSTICK_DRIVER_PRIMITIVE_TYPE_SEMIAXIS:
      {
        pElement->SetAttribute(BUTTONMAP_XML_ATTR_FEATURE_AXIS, strPrimitive);
        break;
      }
      case JOYSTICK_DRIVER_PRIMITIVE_TYPE_MOTOR:
      {
        pElement->SetAttribute(BUTTONMAP_XML_ATTR_FEATURE_MOTOR, strPrimitive);
        break;
      }
      default:
        break;
    }
  }
}

bool CButtonMapXml::Deserialize(const TiXmlElement* pElement, FeatureVector& features)
{
  const TiXmlElement* pFeature = pElement->FirstChildElement(BUTTONMAP_XML_ELEM_FEATURE);

  if (!pFeature)
  {
    esyslog("Can't find <%s> tag", BUTTONMAP_XML_ELEM_FEATURE);
    return false;
  }

  while (pFeature)
  {
    const char* name = pFeature->Attribute(BUTTONMAP_XML_ATTR_FEATURE_NAME);
    if (!name)
    {
      esyslog("<%s> tag has no \"%s\" attribute", BUTTONMAP_XML_ELEM_FEATURE, BUTTONMAP_XML_ATTR_FEATURE_NAME);
      return false;
    }
    std::string strName(name);

    const TiXmlElement* pUp = nullptr;
    const TiXmlElement* pDown = nullptr;
    const TiXmlElement* pRight = nullptr;
    const TiXmlElement* pLeft = nullptr;

    const TiXmlElement* pPositiveX = nullptr;
    const TiXmlElement* pPositiveY = nullptr;
    const TiXmlElement* pPositiveZ = nullptr;

    // Determine the feature type
    JOYSTICK_FEATURE_TYPE type;

    ADDON::DriverPrimitive primitive;
    if (DeserializePrimitive(pFeature, primitive, strName))
    {
      type = JOYSTICK_FEATURE_TYPE_SCALAR;
    }
    else
    {
      pUp = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_UP);
      pDown = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_DOWN);
      pRight = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_RIGHT);
      pLeft = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_LEFT);

      if (pUp || pDown || pRight || pLeft)
      {
        type = JOYSTICK_FEATURE_TYPE_ANALOG_STICK;
      }
      else
      {
        pPositiveX = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_POSITIVE_X);
        pPositiveY = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_POSITIVE_Y);
        pPositiveZ = pFeature->FirstChildElement(BUTTONMAP_XML_ELEM_POSITIVE_Z);

        if (pPositiveX || pPositiveY || pPositiveZ)
        {
          type = JOYSTICK_FEATURE_TYPE_ACCELEROMETER;
        }
        else
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_FEATURE);
          return false;
        }
      }
    }

    ADDON::JoystickFeature feature(strName, type);

    // Deserialize according to type
    switch (type)
    {
      case JOYSTICK_FEATURE_TYPE_SCALAR:
      {
        feature.SetPrimitive(JOYSTICK_SCALAR_PRIMITIVE, primitive);
        break;
      }
      case JOYSTICK_FEATURE_TYPE_ANALOG_STICK:
      {
        ADDON::DriverPrimitive up;
        ADDON::DriverPrimitive down;
        ADDON::DriverPrimitive right;
        ADDON::DriverPrimitive left;

        bool bSuccess = true;

        if (pUp && !DeserializePrimitive(pUp, up, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_UP);
          bSuccess = false;
        }

        if (pDown && !DeserializePrimitive(pDown, down, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_DOWN);
          bSuccess = false;
        }

        if (pRight && !DeserializePrimitive(pRight, right, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_RIGHT);
          bSuccess = false;
        }

        if (pLeft && !DeserializePrimitive(pLeft, left, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_LEFT);
          bSuccess = false;
        }

        if (!bSuccess)
          return false;

        feature.SetPrimitive(JOYSTICK_ANALOG_STICK_UP, up);
        feature.SetPrimitive(JOYSTICK_ANALOG_STICK_DOWN, down);
        feature.SetPrimitive(JOYSTICK_ANALOG_STICK_RIGHT, right);
        feature.SetPrimitive(JOYSTICK_ANALOG_STICK_LEFT, left);

        break;
      }
      case JOYSTICK_FEATURE_TYPE_ACCELEROMETER:
      {
        ADDON::DriverPrimitive positiveX;
        ADDON::DriverPrimitive positiveY;
        ADDON::DriverPrimitive positiveZ;

        bool bSuccess = true;

        if (pPositiveX && !DeserializePrimitive(pPositiveX, positiveY, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_POSITIVE_X);
          bSuccess = false;
        }

        if (pPositiveY && !DeserializePrimitive(pPositiveY, positiveY, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_POSITIVE_Y);
          bSuccess = false;
        }

        if (pPositiveZ && !DeserializePrimitive(pPositiveZ, positiveZ, strName))
        {
          esyslog("Feature \"%s\": <%s> tag is not a valid primitive", strName.c_str(), BUTTONMAP_XML_ELEM_POSITIVE_Z);
          bSuccess = false;
        }

        if (!bSuccess)
          return false;

        feature.SetPrimitive(JOYSTICK_ACCELEROMETER_POSITIVE_X, positiveX);
        feature.SetPrimitive(JOYSTICK_ACCELEROMETER_POSITIVE_Y, positiveY);
        feature.SetPrimitive(JOYSTICK_ACCELEROMETER_POSITIVE_Z, positiveZ);

        break;
      }
      default:
        break;
    }

    features.push_back(feature);

    pFeature = pFeature->NextSiblingElement(BUTTONMAP_XML_ELEM_FEATURE);
  }

  return true;
}

bool CButtonMapXml::DeserializePrimitive(const TiXmlElement* pElement, ADDON::DriverPrimitive& primitive, const std::string& featureName)
{
  const char* button = pElement->Attribute(BUTTONMAP_XML_ATTR_FEATURE_BUTTON);
  if (button)
  {
    primitive = ButtonMapTranslator::ToDriverPrimitive(button, JOYSTICK_DRIVER_PRIMITIVE_TYPE_BUTTON);
  }
  else
  {
    const char* hat = pElement->Attribute(BUTTONMAP_XML_ATTR_FEATURE_HAT);
    if (hat)
    {
      primitive = ButtonMapTranslator::ToDriverPrimitive(hat, JOYSTICK_DRIVER_PRIMITIVE_TYPE_HAT_DIRECTION);
    }
    else
    {
      const char* axis = pElement->Attribute(BUTTONMAP_XML_ATTR_FEATURE_AXIS);
      if (axis)
      {
        primitive = ButtonMapTranslator::ToDriverPrimitive(axis, JOYSTICK_DRIVER_PRIMITIVE_TYPE_SEMIAXIS);
      }
      else
      {
        const char* motor = pElement->Attribute(BUTTONMAP_XML_ATTR_FEATURE_MOTOR);
        if (motor)
        {
          primitive = ButtonMapTranslator::ToDriverPrimitive(motor, JOYSTICK_DRIVER_PRIMITIVE_TYPE_MOTOR);
        }
        else
        {
          return false;
        }
      }
    }
  }

  return true;
}
