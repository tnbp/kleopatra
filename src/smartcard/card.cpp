/*  smartcard/card.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 Intevation GmbH

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "readerstatus.h"
#include "card.h"

using namespace Kleo;
using namespace Kleo::SmartCard;

Card::Card(): mCanLearn(false),
              mHasNullPin(false),
              mStatus(Status::NoCard),
              mAppType(UnknownApplication),
              mAppVersion(-1) {
}

void Card::setStatus(Status s)
{
    mStatus = s;
}

Card::Status Card::status() const
{
    return mStatus;
}

void Card::setSerialNumber(const std::string &sn)
{
    mSerialNumber = sn;
}

std::string Card::serialNumber() const
{
    return mSerialNumber;
}

Card::AppType Card::appType() const
{
    return mAppType;
}

void Card::setAppType(AppType t)
{
    mAppType = t;
}

void Card::setAppVersion(int version)
{
    mAppVersion = version;
}

int Card::appVersion() const
{
    return mAppVersion;
}

std::vector<Card::PinState> Card::pinStates() const
{
    return mPinStates;
}

void Card::setPinStates(std::vector<PinState> pinStates)
{
    mPinStates = pinStates;
}

void Card::setSlot(int slot)
{
    mSlot = slot;
}

int Card::slot() const
{
    return mSlot;
}

bool Card::hasNullPin() const
{
    return mHasNullPin;
}

void Card::setHasNullPin(bool value)
{
    mHasNullPin = value;
}

bool Card::canLearnKeys() const
{
    return mCanLearn;
}

void Card::setCanLearnKeys(bool value)
{
    mCanLearn = value;
}

bool Card::operator == (const Card& other) const
{
    return mStatus == other.status()
        && mSerialNumber == other.serialNumber()
        && mAppType == other.appType()
        && mAppVersion == other.appVersion()
        && mPinStates == other.pinStates()
        && mSlot == other.slot()
        && mCanLearn == other.canLearnKeys()
        && mHasNullPin == other.hasNullPin();
}

bool Card::operator != (const Card& other) const
{
    return !operator==(other);
}