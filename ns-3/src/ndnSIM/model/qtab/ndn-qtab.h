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

#ifndef _NDN_QTAB_H_
#define	_NDN_QTAB_H_

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

#include "ndn-qtab-entry.h"

namespace ns3 {
namespace ndn {

class L3Protocol;
class Face;
class ContentObject;
class Interest;

typedef Interest InterestHeader;
typedef ContentObject ContentObjectHeader;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/**
 * \ingroup ndn
 * \brief Class implementing Pending Interests Table
 */
class Qtab : public Object
{
public:
  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId GetTypeId ();

  /**
   * \brief QTAB constructor
   */
  Qtab ();

  /**
   * \brief Destructor
   */
  virtual ~Qtab ();

  /**
   * \brief Find corresponding QTAB entry for the given content name
   *
   * Not that this call should be repeated enough times until it return 0.
   * This way all records with shorter or equal prefix as in content object will be found
   * and satisfied.
   *
   * \param prefix Prefix for which to lookup the entry
   * \returns smart pointer to PIT entry. If record not found,
   *          returns 0
   */
  virtual Ptr<qtab::Entry>
  LookupQtab (const ContentObject &header) = 0;

  /**
   * \brief Find a PIT entry for the given content interest
   * \param header parsed interest header
   * \returns iterator to Pit entry. If record not found,
   *          return end() iterator
   */
  virtual Ptr<qtab::Entry>
  LookupQtab (const Interest &header) = 0;

  /**
   * @brief Get PIT entry for the prefix (exact match)
   *
   * @param prefix Name for PIT entry
   * @returns If entry is found, a valid iterator (Ptr<pit::Entry>) will be returned. Otherwise End () (==0)
   */
  virtual Ptr<qtab::Entry>
  FindQtab (const Name &prefix) = 0;

  /**
   * @brief Creates a PIT entry for the given interest
   * @param header parsed interest header
   * @returns iterator to Pit entry. If record could not be created (e.g., limit reached),
   *          return end() iterator
   *
   * Note. This call assumes that the entry does not exist (i.e., there was a Lookup call before)
   */
  virtual Ptr<qtab::Entry>
  CreateQtab (Ptr<const Interest> header, Ptr<fib::Entry> fibEntry, uint32_t interfaces) = 0;

  /**
   * @brief Mark PIT entry deleted
   * @param entry PIT entry
   *
   * Effectively, this method removes all incoming/outgoing faces and set
   * lifetime +m_PitEntryDefaultLifetime from Now ()
   */
  virtual void
  MarkErasedQtab (Ptr<qtab::Entry> entry) = 0;

  /**
   * @brief Print out PIT contents for debugging purposes
   *
   * Note that there is no definite order in which entries are printed out
   */
  virtual void
  PrintQtab (std::ostream &os) const = 0;

  /**
   * @brief Get number of entries in PIT
   */
  virtual uint32_t
  GetSizeQtab () const = 0;

  /**
   * @brief Return first element of FIB (no order guaranteed)
   */
  virtual Ptr<qtab::Entry>
  BeginQtab () = 0;

  /**
   * @brief Return item next after last (no order guaranteed)
   */
  virtual Ptr<qtab::Entry>
  EndQtab () = 0;

  /**
   * @brief Advance the iterator
   */
  virtual Ptr<qtab::Entry>
  NextQtab (Ptr<qtab::Entry>) = 0;

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Static call to cheat python bindings
   */
  static inline Ptr<Qtab>
  GetQtab (Ptr<Object> node);

  /**
   * @brief Get max amount of time for Exploit phase.
   */
  inline const Time&
  GetQtabEntryLifetime () const;

  /**
   * @brief Get max amount of time for Exploit phase.
   */
  inline void
  SetQtabEntryLifetime (const Time &entryLifetime);

  /**
   * @brief Get max number of chunks for Exploration phase.
   */
  inline const uint32_t
  GetMaxChunksExploration () const;

  /**
   * @brief Set max number of chunks for Exploration phase.
   */
  inline void
  SetMaxChunksExploration (const uint32_t maxChunks);

  /**
   * @brief Get max number of chunks for Exploitation phase.
   */
  inline const uint32_t
  GetMaxChunksExploitation () const;

  /**
   * @brief Set max number of chunks for Exploitation phase.
   */
  inline void
  SetMaxChunksExploitation (const uint32_t maxChunks);

  /**
   * @brief Get eta weigth.
   */
  inline const double
  GetEta () const;

  /**
   * @brief Set eta weigth.
   */
  inline void
  SetEta (const double etaValue);

  /**
   * @brief Get eta weigth.
   */
  inline void
  SetDelta (const double deltaValue);

  /**
   * @brief Set eta weigth.
   */
  inline const double
  GetDelta () const;



  inline void
  InitializeRttVect (uint32_t interfaces);

  inline void
  SetRttVect (Time rtt, uint32_t interface);

  inline const std::vector<Time>&
  GetRttVect () const;

protected:
  Time m_QtabEntryLifetime;

  double eta;
  double delta;
  uint32_t numTMax;
  uint32_t numRMax;
  std::vector<Time> rttNeigh;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

inline std::ostream&
operator<< (std::ostream& os, const Qtab &qtab)
{
  qtab.PrintQtab (os);
  return os;
}

inline Ptr<Qtab>
Qtab::GetQtab (Ptr<Object> node)
{
  return node->GetObject<Qtab> ();
}

inline const Time&
Qtab::GetQtabEntryLifetime () const
{
  return m_QtabEntryLifetime;
}

inline void
Qtab::SetQtabEntryLifetime (const Time &entryLifetime)
{
	m_QtabEntryLifetime = entryLifetime;
}

inline const uint32_t
Qtab::GetMaxChunksExploration () const
{
  return numRMax;
}

inline void
Qtab::SetMaxChunksExploration (const uint32_t maxChunks)
{
	numRMax = maxChunks;
}

inline const uint32_t
Qtab::GetMaxChunksExploitation () const
{
  return numTMax;
}

inline void
Qtab::SetMaxChunksExploitation (const uint32_t maxChunks)
{
	numTMax = maxChunks;
}

inline const double
Qtab::GetEta () const
{
  return eta;
}

inline void
Qtab::SetEta (const double etaValue)
{
	eta = etaValue;
}

inline const double
Qtab::GetDelta () const
{
  return delta;
}

inline void
Qtab::SetDelta (const double deltaValue)
{
	delta = deltaValue;
}



inline void
Qtab::InitializeRttVect (uint32_t interfaces)
{
  rttNeigh.resize(interfaces,MicroSeconds(0));
}

inline void
Qtab::SetRttVect (Time rtt, uint32_t interface)
{
  rttNeigh[interface] = rtt;
}

inline const std::vector<Time>&
Qtab::GetRttVect () const
{
	return rttNeigh;
}


} // namespace ndn
} // namespace ns3

#endif	/* NDN_QTAB_H */
