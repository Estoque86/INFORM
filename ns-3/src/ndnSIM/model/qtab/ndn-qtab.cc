/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ndn-qtab.h"

#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"

#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"

#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

NS_LOG_COMPONENT_DEFINE ("ndn.Qtab");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (Qtab);

TypeId
Qtab::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndn::Qtab")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    .AddAttribute ("QtabEntryLifetime",
                   "Maximum amount of time for which a router is willing to maintain a Qtab entry. ",
                   TimeValue (), // by default, PIT entries are kept for the time, specified by the InterestLifetime
                   MakeTimeAccessor (&Qtab::GetQtabEntryLifetime, &Qtab::SetQtabEntryLifetime),
                   MakeTimeChecker ())
    ;

  return tid;
}

Qtab::Qtab ()
{
}

Qtab::~Qtab ()
{
}

} // namespace ndn
} // namespace ns3
