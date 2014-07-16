/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 *         Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "flooding-inform.h"
#include "ns3/ndn-l3-protocol.h"


#include "ns3/ndn-interest.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "../qtab/ndn-qtab.h"
#include "../qtab/ndn-qtab-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {
namespace fw {

NS_OBJECT_ENSURE_REGISTERED (FloodingInform);

LogComponent FloodingInform::g_log = LogComponent (FloodingInform::GetLogName ().c_str ());

std::string
FloodingInform::GetLogName ()
{
  return super::GetLogName ()+".FloodingInform";
}

TypeId FloodingInform::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndn::fw::FloodingInform")
    .SetGroupName ("Ndn")
    .SetParent <Nacks> ()
    .AddConstructor <FloodingInform> ()
    ;
  return tid;
}

FloodingInform::FloodingInform ()
{
}

bool
FloodingInform::DoPropagateInterestQtab (Ptr<Face> inFace,
                               Ptr<const Interest> header,
                               Ptr<const Packet> origPacket,
                               Ptr<pit::Entry> pitEntry,
                               Ptr<qtab::Entry> qtabEntry)
{
  NS_LOG_FUNCTION (this);

  int propagatedCount = 0;

  // If a node has just one interface, checking to Inform status is worthless.

  uint32_t numInterf = inFace->GetNode()->GetNDevices();

  if(numInterf <= 1)
  {
	  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
      {
		  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));
		  if (metricFace.GetStatus () == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
			  break;

		  if (!TrySendOutInterest (inFace, metricFace.GetFace (), header, origPacket, pitEntry))
		  {
			  continue;
		  }
		  propagatedCount++;
      }
  }
  else
  {
	  uint32_t minDelayFace, minQFace, randFace;
	  bool different = false;

	  if(qtabEntry->GetExplorationFlag())     // *** EXPLORATION PHASE
	  {
		  if(qtabEntry->GetIsFirstExplorationFlag())      // FIRST EXPLORATION PHASE  (MinDelay Face + Random)
		  {
			  NS_LOG_UNCOND("FloodINFORM - NODE:\t" << inFace->GetNode()->GetId() << "\t forwarding Interest:\t" << header->GetName() << "\tin FIRST EXPLORATION PHASE...");
			  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
		      {
				  if (metricFace.GetStatus () == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
					  break;

				  if (!TrySendOutInterest (inFace, metricFace.GetFace (), header, origPacket, pitEntry))
				  {
					  continue;
				  }
				  propagatedCount++;

				  minDelayFace = metricFace.GetFace()->GetId();
				  NS_LOG_UNCOND("FloodINFORM - MinDelayFace:\t" << minDelayFace);
		      }

			  // Select Random Face different from the MinDelayOne.   // With only two interfaces, the Interest is forwarder toward the interface different fro the incoming one.
			  while(!different)
			  {
				  randFace = (uint32_t)( (int)( myRand->GetValue()*pow(10,5) ) % numInterf);
				  if(randFace != minDelayFace && randFace != inFace->GetId())
					  different = true;
				  else
				  {
					  if(randFace != inFace->GetId())
					  {
						  if(numInterf == 2 && randFace == minDelayFace)
							  different=true;
					  }
				  }
			  }

			  if (randFace != minDelayFace)
			  {
				  NS_LOG_UNCOND("FloodINFORM - RandomFace:\t" << randFace);

				  Ptr<Face> randFacePtr = inFace->GetNode()->GetObject<L3Protocol>()->GetFace(randFace);

				  if (SendOutInterestInform (inFace, randFacePtr, header, origPacket, pitEntry))
				  {
					  propagatedCount++;
				  }

			  }

		  }
		  else				  // Successive EXPLORATION Phases  (MinQValue Face Precedente + Random)
		  {
			  NS_LOG_UNCOND("FloodINFORM - NODE:\t" << inFace->GetNode()->GetId() << "\t forwarding Interest:\t" << header->GetName() << "\tin EXPLORATION PHASE...");

			  minQFace = qtabEntry->GetqMin().interfId;

			  NS_LOG_UNCOND("FloodINFORM - MinQvalueFace:\t" << minQFace);

			  // Select a different random face.
			  while(!different)
			  {
				  randFace = (uint32_t)( (int)( myRand->GetValue()*pow(10,5) ) % numInterf);
				  if(randFace != minQFace && randFace != inFace->GetId())
					  different = true;
				  else
				  {
					  if(randFace != inFace->GetId())
					  {
						  if(numInterf == 2 && randFace == minQFace)
							  different=true;
					  }
				  }
			  }

			  Ptr<Face> minQFacePtr = inFace->GetNode()->GetObject<ns3::ndn::L3Protocol>()->GetFace(minQFace);

			  if (SendOutInterestInform (inFace, minQFacePtr, header, origPacket, pitEntry))
			  {
				  propagatedCount++;
			  }


			  if (randFace != minQFace)
			  {
				  NS_LOG_UNCOND("FloodINFORM - RandomFace:\t" << randFace);

				  Ptr<Face> randFacePtr = inFace->GetNode()->GetObject<ns3::ndn::L3Protocol>()->GetFace(randFace);

				  if (SendOutInterestInform (inFace, randFacePtr, header, origPacket, pitEntry))
				  {
					  propagatedCount++;
				  }

			  }
		  }

	  }
	  else		// EXPLOITATION PHASE   (MinQValue Face)
	  {
		  NS_LOG_UNCOND("FloodINFORM - NODE:\t" << inFace->GetNode()->GetId() << "\t forwarding Interest:\t" << header->GetName() << "\tin EXPLOITATION PHASE...");

		  minQFace = qtabEntry->GetqMin().interfId;

		  NS_LOG_UNCOND("FloodINFORM - MinQvalueFace:\t" << minQFace);

		  Ptr<Face> minQFacePtr = inFace->GetNode()->GetObject<ns3::ndn::L3Protocol>()->GetFace(minQFace);
		  if (SendOutInterestInform (inFace, minQFacePtr, header, origPacket, pitEntry))
		  {
			  propagatedCount++;
		  }
	  }

  }

  NS_LOG_INFO ("Propagated to " << propagatedCount << " faces");
  return propagatedCount > 0;
}


bool
FloodingInform::DoPropagateInterest (Ptr<Face> inFace,
                      Ptr<const Interest> header,
                      Ptr<const Packet> origPacket,
                      Ptr<pit::Entry> pitEntry)
{
	return false;
}

} // namespace fw
} // namespace ndn
} // namespace ns3
