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

#ifndef _NDN_QTAB_ENTRY_H_
#define _NDN_QTAB_ENTRY_H_

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include "ns3/ndn-fib.h"

#include "ndn-qtab-entry-incoming-face.h"
#include "ndn-qtab-entry-outgoing-face.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
// #include <boost/multi_index/composite_key.hpp>
// #include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
// #include <boost/multi_index/mem_fun.hpp>
#include <set>
#include <boost/shared_ptr.hpp>

namespace ns3 {
namespace ndn {

class Qtab;

namespace fw { class Tag; }

namespace qtab {

/// @cond include_hidden
class i_face {};
class i_retx {};
/// @endcond

/**
 * \ingroup ndn
 * \brief Typedef for indexed face container of PitEntryOutgoingFace
 *
 * Indexes:
 * - by face (may be it will be possible to replace with just the std::map)
 */
// struct OutgoingFaceContainer
// {
//   /// @cond include_hidden
//   typedef boost::multi_index::multi_index_container<
//     OutgoingFace,
//     boost::multi_index::indexed_by<
//       // For fast access to elements using NdnFace
//       boost::multi_index::ordered_unique<
//         boost::multi_index::tag<i_face>,
//         boost::multi_index::member<OutgoingFace, Ptr<Face>, &OutgoingFace::m_face>
//       >
//       // ,
//       // boost::multi_index::ordered_non_unique<
//       //   boost::multi_index::tag<i_retx>,
//       //   boost::multi_index::member<OutgoingFace, uint32_t, &OutgoingFace::m_retxCount>
//       // >
//     >
//    > type;
//   /// @endcond
// };


/**
 * \ingroup ndn
 * \brief structure for PIT entry
 *
 * All set-methods are virtual, in case index rearrangement is necessary in the derived classes
 */
class Entry : public SimpleRefCount<Entry>
{
public:
  typedef std::set< IncomingFace > in_container; ///< @brief incoming faces container type
  typedef in_container::iterator in_iterator;                ///< @brief iterator to incoming faces

  // typedef OutgoingFaceContainer::type out_container; ///< @brief outgoing faces container type
  typedef std::set< OutgoingFace > out_container; ///< @brief outgoing faces container type
  typedef out_container::iterator out_iterator;              ///< @brief iterator to outgoing faces

  typedef std::set< uint32_t > nonce_container;  ///< @brief nonce container type

  struct qMin{
	  qMin (Time _qValueMin, uint32_t _interfId): qValueMin(_qValueMin), interfId(_interfId) {}

	  Time qValueMin;
	  uint32_t interfId;
  };

  qMin* minQValue;


  /**
   * \brief PIT entry constructor
   * \param prefix Prefix of the PIT entry
   * \param offsetTime Relative time to the current moment, representing PIT entry lifetime
   * \param fibEntry A FIB entry associated with the PIT entry
   */
  Entry (Qtab &container, Ptr<const Interest> header, Ptr<fib::Entry> fibEntry, uint32_t numInterfaces);

  /**
   * @brief Virtual destructor
   */
  virtual ~Entry ();

  /**
   * @brief Update lifetime of PIT entry
   *
   * @param lifetime desired lifetime of the pit entry (relative to the Simulator::Now ())
   *
   * This function will update PIT entry lifetime to the maximum of the current lifetime and
   * the lifetime Simulator::Now () + lifetime
   */
  virtual void
  UpdateQtabEntryLifetime ();

  /**
   * @brief Get prefix of the PIT entry
   */
  const Name &
  GetPrefix () const;

  /**
   * @brief Get current expiration time of the record
   *
   * @returns current expiration time of the record
   */
  const Time &
  GetExpireTime () const;


  /**
   * @brief Add `face` to the list of incoming faces
   *
   * @param face Face to add to the list of incoming faces
   * @returns iterator to the added entry
   */
  virtual in_iterator
  AddIncomingQtab (Ptr<Face> face);

  /**
   * @brief Remove incoming entry for face `face`
   */
  virtual void
  RemoveIncomingQtab (Ptr<Face> face);

  /**
   * @brief Clear all incoming faces either after all of them were satisfied or NACKed
   */
  virtual void
  ClearIncomingQtab ();

  /**
   * @brief Add `face` to the list of outgoing faces
   *
   * @param face Face to add to the list of outgoing faces
   * @returns iterator to the added entry
   */
  virtual out_iterator
  AddOutgoingQtab (Ptr<Face> face);

  /**
   * @brief Clear all incoming faces either after all of them were satisfied or NACKed
   */
  virtual void
  ClearOutgoingQtab ();

  /**
   * @brief Remove all references to face.
   *
   * This method should be called before face is completely removed from the stack.
   * Face is removed from the lists of incoming and outgoing faces
   */
  virtual void
  RemoveAllReferencesToFaceQtab (Ptr<Face> face);

  /**
   * @brief Get associated list (const reference) of incoming faces
   */
  const in_container &
  GetIncoming () const;

  /**
   * @brief Get associated list (const reference) of outgoing faces
   */
  const out_container &
  GetOutgoing () const;

  /**
   * @brief Get number of outgoing faces (needed for python bindings)
   */
  uint32_t
  GetOutgoingCount () const;

  /**
   * @brief Add new forwarding strategy tag
   */
  inline void
  AddFwTag (boost::shared_ptr< fw::Tag > tag);

  /**
   * @brief Get forwarding strategy tag (tag is not removed)
   */
  template<class T>
  inline boost::shared_ptr< T >
  GetFwTag ();

  /**
   * @brief Remove the forwarding strategy tag
   */
  template<class T>
  inline void
  RemoveFwTag ();

  /**
   * @brief Get Interest (if several interests are received, then nonce is from the first Interest)
   */
  Ptr<const Interest>
  GetInterest () const;

  /**
   * @brief Get the minimum qValue and the associated interface
   */
  qMin &
  GetqMin ();

  /**
   * @brief Get the qValues of each interface
   */
  std::vector<Time> &
  GetqValuesInterf ();

  /**
   * @brief Get the flag which indicates the phase of the algorithm
   */
  bool
  GetExplorationFlag () const;

  /**
   * @brief Set the Exploration Flag (exploration)
   */
  void
  SetExplorationFlag (bool value);


  /**
   * @brief Get the flag which indicates if it is the first exploration (isFirstExploration)
   */
  bool
  GetIsFirstExplorationFlag () const;

  /**
   * @brief Set the flag which indicates if it is the first exploration (isFirstExploration)
   */
  void
  SetIsFirstExplorationFlag (bool value);


  /**
   * @brief Get exploration chunks (numR)
   */
  uint32_t
  GetExplorationChunks () const;

  /**
   * @brief Reset the number of exploration chunks (numR)
   */
  void
  SetExplorationChunks (uint32_t value);

  /**
   * @brief Increment the number of exploration chunks (numR)
   */
  void
  IncrementExplorationChunks ();

  void
  DecrementExplorationChunks ();


  /**
   * @brief Get exploitation chunks (numT)
   */
  uint32_t
  GetExploitationChunks () const;

  /**
   * @brief Reset the number of exploitation chunks (numT)
   */
  void
  SetExploitationChunks (uint32_t value);

  /**
   * @brief Get exploitation chunks (numT)
   */
  void
  IncrementExploitationChunks ();

  /**
   * @brief Get the interface associated to the mindelay path toward the known seed copy.
   */
  Ptr<Face>
  GetMinDelayFace () const;

  /**
   * @brief Set the interface associated to the mindelay path toward the known seed copy.
   */
  void
  SetMinDelayFace (Ptr<fib::Entry> entry);

  /**
   * @brief Update the specified qValue
   */
  void
  UpdateQvalue (uint32_t incomingFace, Time piggyQvalue);

  /**
   * @brief Extract the minimum qValue to be piggybacked.
   */
  Time
  ExtractTempMinQvalue ();

  /**
   * @brief Calculate the minimum qValue when entering the exploitation phase.
   */
  void
  CalculateMinQvalue ();

  bool
  CheckContinueExploit (uint32_t incIntId);





private:
  friend std::ostream& operator<< (std::ostream& os, const Entry &entry);

protected:
  Qtab &m_container; ///< @brief Reference to the container (to rearrange indexes, if necessary)

  Ptr<const Interest> m_interest; ///< \brief Interest of the PIT entry (if several interests are received, then nonce is from the first Interest)
  //Ptr<fib::Entry> m_fibEntry;     ///< \brief FIB entry related to this prefix

  /////*****/////QTAB
  std::vector<Time> qValuesInterf;        // Vettore dei qValue associati alle singole interfacce.


  bool exploration;
  bool isFirstExploration;

  uint32_t numR;     			// Exploration chunks
  uint32_t numT;				// Exploitation chunks


  Ptr<Face> minDelayFace;

  in_container  m_incoming;      ///< \brief container for incoming interests
  out_container m_outgoing;      ///< \brief container for outgoing interests

  Time m_qTabEntryExpireTime;         ///< \brief Time when PIT entry will be removed

  uint32_t numberOfInterfaces;

  std::list< boost::shared_ptr<fw::Tag> > m_fwTags; ///< @brief Forwarding strategy tags
};

struct EntryIsNotEmpty
{
  bool
  operator () (Ptr<Entry> entry)
  {
    return !entry->GetIncoming ().empty ();
  }
};

std::ostream& operator<< (std::ostream& os, const Entry &entry);

inline void
Entry::AddFwTag (boost::shared_ptr< fw::Tag > tag)
{
  m_fwTags.push_back (tag);
}

/**
 * @brief Get and remove forwarding strategy tag
 */
template<class T>
inline boost::shared_ptr< T >
Entry::GetFwTag ()
{
  for (std::list< boost::shared_ptr<fw::Tag> >::iterator item = m_fwTags.begin ();
       item != m_fwTags.end ();
       item ++)
    {
      boost::shared_ptr< T > retPtr = boost::dynamic_pointer_cast<T> (*item);
      if (retPtr != boost::shared_ptr< T > ())
        {
          return retPtr;
        }
    }

  return boost::shared_ptr< T > ();
}

// /**
//  * @brief Peek the forwarding strategy tag
//  */
// template<class T>
// inline boost::shared_ptr< const T >
// Entry::PeekFwTag () const
// {
//   for (std::list< boost::shared_ptr<fw::Tag> >::const_iterator item = m_fwTags.begin ();
//        item != m_fwTags.end ();
//        item ++)
//     {
//       boost::shared_ptr< const T > retPtr = boost::dynamic_pointer_cast<const T> (*item);
//       if (retPtr != 0)
//         {
//           return retPtr;
//         }
//     }

//   return 0;
// }

template<class T>
inline void
Entry::RemoveFwTag ()
{
  for (std::list< boost::shared_ptr<fw::Tag> >::iterator item = m_fwTags.begin ();
       item != m_fwTags.end ();
       item ++)
    {
      boost::shared_ptr< T > retPtr = boost::dynamic_pointer_cast< T > (*item);
      if (retPtr != boost::shared_ptr< T > ())
        {
          m_fwTags.erase (item);
          return;
        }
    }
}


} // namespace qtab
} // namespace ndn
} // namespace ns3

#endif // _NDN_QTAB_ENTRY
