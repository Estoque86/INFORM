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

#include "ndn-qtab-entry.h"

#include "ndn-qtab.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-interest.h"

#include "ns3/simulator.h"
#include "ns3/log.h"

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/foreach.hpp>
namespace ll = boost::lambda;

NS_LOG_COMPONENT_DEFINE ("ndn.qtab.Entry");

namespace ns3 {
namespace ndn {
namespace qtab {

Entry::Entry (Qtab &container,
              Ptr<const Interest> header,
              Ptr<fib::Entry> fibEntry,
              uint32_t numInterfaces)
  : m_container (container)
  , m_interest (header)
{
  NS_LOG_FUNCTION (this);

  // Inizializzo la tabella dei qValue
  qValuesInterf.resize(numInterfaces, Time (std::numeric_limits<int64_t>::max ()));

  // Inizializzo il minQvalue
  minQValue = new qMin(MicroSeconds(0), 0);

  numberOfInterfaces = numInterfaces;

  // Setto i flag che indicano lo stato dell'algoritmo per questa entry.
  SetExplorationFlag(true);
  SetIsFirstExplorationFlag(true);

  // Setto i contatori dei chunk delle rispettive fasi dell'algoritmo.
  SetExplorationChunks(0);
  SetExploitationChunks(0);

  // Setto l'interfaccia associato al percorso con il min delay calcolato verso la copia seed.
  SetMinDelayFace(fibEntry);

  UpdateQtabEntryLifetime ();
}

Entry::~Entry ()
{
  NS_LOG_FUNCTION (GetPrefix ());
}

void
Entry::UpdateQtabEntryLifetime ()
{
  NS_LOG_FUNCTION (this);

  Time newExpireTime = Simulator::Now () + m_container.GetQtabEntryLifetime ();

  if (newExpireTime > m_qTabEntryExpireTime)
	  m_qTabEntryExpireTime = newExpireTime;

  //NS_LOG_INFO (this->GetPrefix () << ", Updated lifetime to " << m_expireTime.ToDouble (Time::S) << "s, " << (m_expireTime-Simulator::Now ()).ToDouble (Time::S) << "s left");
}

const Name &
Entry::GetPrefix () const
{
  return m_interest->GetName ();
}

const Time &
Entry::GetExpireTime () const
{
  return m_qTabEntryExpireTime;
}


Entry::in_iterator
Entry::AddIncomingQtab (Ptr<Face> face)
{
  std::pair<in_iterator,bool> ret =
    m_incoming.insert (IncomingFace (face));

  // NS_ASSERT_MSG (ret.second, "Something is wrong");

  return ret.first;
}

void
Entry::RemoveIncomingQtab (Ptr<Face> face)
{
  m_incoming.erase (face);
}

void
Entry::ClearIncomingQtab ()
{
  m_incoming.clear ();
}

Entry::out_iterator
Entry::AddOutgoingQtab (Ptr<Face> face)
{
  std::pair<out_iterator,bool> ret =
    m_outgoing.insert (OutgoingFace (face));

  if (!ret.second)
    { // outgoing face already exists
      const_cast<OutgoingFace&>(*ret.first).UpdateOnRetransmit ();
      // m_outgoing.modify (ret.first,
      //                    ll::bind (&OutgoingFace::UpdateOnRetransmit, ll::_1));
    }

  return ret.first;
}

void
Entry::ClearOutgoingQtab ()
{
  m_outgoing.clear ();
}

void
Entry::RemoveAllReferencesToFaceQtab (Ptr<Face> face)
{
  in_iterator incoming = m_incoming.find (face);

  if (incoming != m_incoming.end ())
    m_incoming.erase (incoming);

  out_iterator outgoing =
    m_outgoing.find (face);

  if (outgoing != m_outgoing.end ())
    m_outgoing.erase (outgoing);
}


const Entry::in_container &
Entry::GetIncoming () const
{
  return m_incoming;
}

const Entry::out_container &
Entry::GetOutgoing () const
{
  return m_outgoing;
}

uint32_t
Entry::GetOutgoingCount () const
{
  return m_outgoing.size ();
}

Ptr<const Interest>
Entry::GetInterest () const
{
  return m_interest;
}


// *** FUNZIONI QTAB ****
bool
Entry::GetExplorationFlag () const
{
  return exploration;
}

void
Entry::SetExplorationFlag (bool value)
{
  exploration = value;
}

bool
Entry::GetIsFirstExplorationFlag () const
{
  return isFirstExploration;
}

void
Entry::SetIsFirstExplorationFlag (bool value)
{
  isFirstExploration = value;
}

uint32_t
Entry::GetExplorationChunks () const
{
  return numR;
}

void
Entry::SetExplorationChunks (uint32_t value)
{
  numR = value;
}

void
Entry::IncrementExplorationChunks ()
{
  numR++;
}

void
Entry::DecrementExplorationChunks ()
{
  numR--;
}


uint32_t
Entry::GetExploitationChunks () const
{
  return numT;
}

void
Entry::SetExploitationChunks (uint32_t value)
{
  numT = value;
}

void
Entry::IncrementExploitationChunks ()
{
  numT++;
}

Ptr<Face>
Entry::GetMinDelayFace () const
{
	return minDelayFace;
}

void
Entry::SetMinDelayFace (Ptr<fib::Entry> fibEntry)
{
	minDelayFace = fibEntry->FindBestCandidate(0).GetFace();
}

void
Entry::UpdateQvalue (uint32_t incomingFace, Time piggyQvalue)
{
	double eta = m_container.GetEta();
	Time rtt = m_container.GetRttVect().operator [](incomingFace);
	if(qValuesInterf[incomingFace] == Time (std::numeric_limits<int64_t>::max ()))
		qValuesInterf[incomingFace] =  piggyQvalue + rtt;
	else
	{
		double value = qValuesInterf[incomingFace].ToDouble(Time::US);
		double piggyQvalueDouble = piggyQvalue.ToDouble(Time::US);
		double rttDouble = rtt.ToDouble(Time::US);
		double newValue = (1 - eta) * value + eta * (piggyQvalueDouble + rttDouble);
		uint64_t newTime = static_cast<uint64_t>(newValue);
		Time newTimeValue = MicroSeconds(newTime);
		qValuesInterf[incomingFace] = newTimeValue;
	}
		//qValuesInterf[incomingFace] =  (1 - eta) * qValuesInterf[incomingFace] + eta * (piggyQvalue + rtt);
}

Time
Entry::ExtractTempMinQvalue ()
{
	std::vector<Time>::iterator iterMin;
	//uint32_t intMin;

	iterMin = std::min_element(qValuesInterf.begin(), qValuesInterf.end());
	if(*iterMin!=Time (std::numeric_limits<int64_t>::max ()))   // Se la qTabEntry è presente, vuol dire che ho recuperato almeno un chunk del relativo contenuto, e che quindi ho aggiornato il qValue.
	{
		//intMin = std::distance(qValuesInterf.begin(), max_element(qValuesInterf.begin(), qValuesInterf.end()));
		return *iterMin;
	}
	else
	{
		return NanoSeconds(0);
		NS_LOG_UNCOND("ERRORE!! - Un qValue deve essere necessariamente diverso da MAX");
	}
}

void
Entry::CalculateMinQvalue ()
{
	std::vector<Time>::iterator iterMin;
	uint32_t intMin;

	iterMin = std::min_element(qValuesInterf.begin(), qValuesInterf.end());
	if(*iterMin!=Time (std::numeric_limits<int64_t>::max ()))   // Se la qTabEntry è presente, vuol dire che ho recuperato almeno un chunk del relativo contenuto, e che quindi ho aggiornato il qValue.
	{
		intMin = std::distance(qValuesInterf.begin(), std::min_element(qValuesInterf.begin(), qValuesInterf.end()));
		//return *iterMin;
	}
	else
	{
		//return NanoSeconds(0);
		NS_LOG_UNCOND("ERRORE!! - Durante la exploration phase, non è stato aggiornato nessun qValue.");
		return;
	}
	minQValue->interfId = intMin;
	minQValue->qValueMin = *iterMin;
}

bool
Entry::CheckContinueExploit (uint32_t incIntId)
{
	double qMinDouble = static_cast<double> (minQValue->qValueMin.ToDouble (Time::US));
	double qIncIntDouble = static_cast<double> (qValuesInterf[incIntId].ToDouble (Time::US));

	double valueChecked = (std::abs(qMinDouble-qIncIntDouble)/qMinDouble);

	if(valueChecked > m_container.GetDelta())
		return false;
	else
		return true;
}

Entry::qMin&
Entry::GetqMin ()
{
	return *minQValue;
}

std::vector<Time> &
Entry::GetqValuesInterf ()
{
	return qValuesInterf;
}





std::ostream& operator<< (std::ostream& os, const Entry &entry)
{
  os << "Prefix: " << entry.GetPrefix () << "\n";
  os << "In: ";
  bool first = true;
  BOOST_FOREACH (const IncomingFace &face, entry.m_incoming)
    {
      if (!first)
        os << ",";
      else
        first = false;

      os << *face.m_face;
    }

  os << "\nOut: ";
  first = true;
  BOOST_FOREACH (const OutgoingFace &face, entry.m_outgoing)
    {
      if (!first)
        os << ",";
      else
        first = false;

      os << *face.m_face;
    }

  return os;
}


} // namespace pit
} // namespace ndn
} // namespace ns3
